/*
 *	file name : /hw1-nabdul/hw1/sys_xconcat.c
 *
 *	author    : Nafees Ahmed Abdul
 *
 *	date	  : 9th March 2014
 *
 *	File description : System call which replicates the behaviour of cat
 *			   system call  by concatenating one or more files into a 
 *			   destination target file by considering all the
 * 			    options that user has specified. 		
 */
#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#define PAGE_SIZE_CHUNK 4096
asmlinkage extern long (*sysptr)(void *arg);


/*  process_details -   structure which defines the arguments passed 
 *		        from user to system call
 *  @outfile		name of output file
 *  @infiles    	array with names of input files
 *  @infile_count 	number of input files in infiles array
 *  @oflags	        open flags to change behavior of system call
 *  @mode               default permission mode for newly created outfile
 *  @flags	        special flags to alter the behavior of system call 			
 *
 */
struct process_details {
	__user const char *outfile;
	__user const char **infiles;
	unsigned int infile_count;
	int oflags;
	mode_t mode;
	unsigned int flags;
};

static int validation(struct process_details *current_process);
static int concat(struct process_details *current_process, void *buf);

/**
 * xconcat -   system call implementation to concatenate files and 
 *	       output result to either destination file or return error
 *	       to user program in case of errors 
 * @args:      Buffer pointer that holds all the arguments from user
 * @argslen:   Size of above args
 *
 * This method holds the actual implementation of system call which in 
 * turns calls various functions to read/write the content to output 
 * and to validate the arguments from user
 *
 */
asmlinkage int xconcat(void *args, int argslen)
{
	void *buf;
	int inputcheck;
	struct process_details *current_process;
	int retval;
	int ret;
	current_process = kmalloc(argslen, GFP_KERNEL);
	/*Allocate buf size with PAGE_SIZE that kernel should actually access*/
	buf = kmalloc(PAGE_SIZE_CHUNK, GFP_KERNEL);
	if (!buf) {
		retval = -ENOMEM;
		printk(KERN_ERR "\n Error: problem with memory allocation");
		goto out;
	}

	/*check whether argslen is equal to valid struct size*/
	if (argslen != sizeof(struct process_details)) {
		retval = -EINVAL;
		printk(KERN_ERR "\n Error: Invalid arguments passed");
		goto out_invalidargs;
	}

	/*check the validity of user address space */
	ret = copy_from_user(current_process, args, argslen);
	if (ret != 0) {
		retval = -EFAULT;
		printk("\n Error: Invalid user address space");
		goto out_invalidargs;
	}
	inputcheck = validation(current_process);
	/*If return value not successful */
	if (inputcheck != 1) {
		retval = inputcheck;
		goto out_invalidargs;
	}
	retval = concat(current_process, buf);
	/*Handling memory leaks */
out_invalidargs:
		kfree(buf);
out:
		return retval;
}

/**
 * concat -            system call implementation to concatenate files and 
 *                     output result to either destination file or return error
 *                     to user program in case of errors 
 * @current_process:   variable containing all the arguments in form of 
 *		       struct process_details type. 
 * @buf            :   Buffer memory allocated with respective to chunks
 *		       PAGE SIZE of particular architecture which inturn
 */
static int concat(struct process_details *current_process, void *buf)
{
	int rbytes;
	int wbytes;
	/*Variable to track input files that are opened properly */
	int m = 0;
	int success_files = 0;
	int success_bytes = 0;
	struct kstat stat;
	int totalsize = 0;
	struct file *rfilp[current_process->infile_count];
	struct file *wfilp = NULL;
	mm_segment_t oldfs;
	int retval;
	/*length to be read is always in chunks of page size */
	int len = PAGE_SIZE_CHUNK;
	int fcounter;
	int atomic;
	#ifdef EXTRA_CREDIT
	mode_t temp_mode;
	struct file *temp;
	char *tempout = "tempout.tmp";
	int vfs_status = 0;
	struct dentry *temp_dentry = NULL;
	struct inode *temp_dir = NULL;
	#endif
	struct dentry *w_dentry = NULL;
	struct inode *w_dir = NULL;
	int flag = 0;
	int cflag = 0;
	m = success_files = current_process->infile_count;

	if (current_process->flags & 0X04)
		atomic = 1;
	else
		atomic = 0;
	/*
	 * open all the files and validate each of them
	 */
	for (fcounter = 0; fcounter < current_process->infile_count;
						 ++fcounter) {

		rfilp[fcounter] = filp_open(current_process->infiles[fcounter],
					O_RDONLY, 0);

		if (!rfilp[fcounter] || IS_ERR(rfilp[fcounter])) {
			printk("\n Error: Opening input files ");
			retval = (int) PTR_ERR(rfilp[fcounter]);
			m = fcounter;
			goto out_closeoutput;

		}

		if (!rfilp[fcounter]->f_op->read) {
			retval = (int) PTR_ERR(rfilp[fcounter]);
			printk("\n Error: File doesn't allow reads.");
			m = fcounter;
			goto out_closeoutput;
		}

	}
if (atomic) {
#ifdef EXTRA_CREDIT
	/*
	 *To preserve the output file when the process
	 *fails mid way in truncate mode
         */
	if ((current_process->oflags & O_CREAT) == O_CREAT &&
			(current_process->oflags & O_EXCL) == O_EXCL) {
		current_process->oflags = current_process->flags ^ O_EXCL;
	}


	if (current_process->oflags & O_TRUNC) {
                wfilp = filp_open(current_process->outfile, O_RDWR,
				current_process->mode);
	}
	else {
		wfilp = filp_open(current_process->outfile,
				current_process->oflags,current_process->mode);
	}

	if (!wfilp || IS_ERR(wfilp)) {
		retval = (int) PTR_ERR(wfilp);	
		printk(KERN_ERR "\nError : Opening output file");
		goto out_closeoutput;
	}


	if (!wfilp->f_op->write) {
		retval = (int) PTR_ERR(wfilp);
   		printk(KERN_ERR "\n Error: File doesn't allow writes");
		goto out_closeoutput;
        }


	oldfs = get_fs();
	set_fs(KERNEL_DS);
	temp_mode = wfilp->f_dentry->d_inode->i_mode;
	temp = filp_open(tempout, O_RDWR | O_CREAT, temp_mode);
	 if (current_process->oflags & O_APPEND) {
		do {   
			rbytes = vfs_read(wfilp, buf , len, &wfilp->f_pos);
			if (rbytes < 0) {
				printk(KERN_ERR "\n Error:reading output file into temporary file atomic mode");
				retval = rbytes;
				goto out;
           		}
			wbytes = vfs_write(temp, buf, rbytes, &temp->f_pos);
			if (wbytes < 0) {
				printk(KERN_ERR "\n Error:writing into temporary file atomic mode");
				retval = wbytes;
				goto out;
			}
		 } while (rbytes >=len);
        }
	set_fs(oldfs);
	temp_dir = temp->f_dentry->d_parent->d_inode;
	temp_dentry = temp->f_dentry;
	w_dir = wfilp->f_dentry->d_parent->d_inode;
	w_dentry = wfilp->f_dentry;
	for (fcounter=0; fcounter < current_process->infile_count; ++fcounter)
	{
		rfilp[fcounter]->f_pos = 0;
		oldfs = get_fs();
		set_fs(KERNEL_DS);
		do
		{
			rbytes = vfs_read(rfilp[fcounter], buf, len,
					 &rfilp[fcounter]->f_pos);
			if (rbytes < 0) {
				filp_close(temp, NULL);
				vfs_unlink(temp_dir, temp_dentry);
				cflag = 1;
				m = fcounter;
				retval = rbytes;
				goto out;
			}
			wbytes = vfs_write(temp, buf, rbytes, &temp->f_pos);
			if (wbytes < 0) {
				filp_close(temp,NULL);
				vfs_unlink(temp_dir, temp_dentry);
				cflag = 1;
				m = fcounter;
				retval= wbytes;
				goto out;
			}
			success_bytes += wbytes;
		} while (rbytes >= len);
		set_fs(oldfs);
	}
	filp_close(wfilp, NULL);
	vfs_status = vfs_unlink(w_dir, w_dentry);
	if (vfs_status != 0) {
		retval = vfs_status;
		printk(KERN_ERR "\n Error : VFS_unlink Failed");
		goto out_closeoutput;
	}
	vfs_status = vfs_rename(temp_dir, temp_dentry, w_dir,w_dentry);
	if (vfs_status != 0 ) {
		retval = vfs_status;
		printk(KERN_ERR "\n Error : VFS_rename Failed");
		goto out_closeoutput;
	}
#else
	retval = -EINVAL;
	printk(KERN_ERR "\n Error : Extra Credit mode is turned off");
	goto out_closeoutput;

#endif
}
else{
	if ((current_process->oflags & O_CREAT ) == O_CREAT &&
			 (current_process->oflags & O_EXCL) == O_EXCL) {
		wfilp = filp_open(current_process->outfile, current_process->oflags ^ O_EXCL,
                                current_process->mode);
	}
	else {
        	wfilp = filp_open(current_process->outfile, current_process->oflags,
      				                       current_process->mode);
	}

	if (!wfilp || IS_ERR(wfilp)) {
		retval = (int) PTR_ERR(wfilp);
		printk(KERN_ERR "\nError : Opening output file");
		goto out_closeoutput;
        }

	if (!wfilp->f_op->write) {
		retval = (int) PTR_ERR(wfilp);
		printk(KERN_ERR "\n Error: File doesn't allow writes");
		goto out;
	}
	for (fcounter = 0; fcounter < current_process->infile_count; ++fcounter) {
		rfilp[fcounter]->f_pos = 0;
		oldfs = get_fs();
		set_fs(KERNEL_DS);
		do {
			rbytes = vfs_read(rfilp[fcounter], buf, len,
					&rfilp[fcounter]->f_pos);
			if (rbytes < 0) {
				/*failed midway do not consider as error*/
				success_files = fcounter;
				flag = 1;
				break;
			}
			wbytes = vfs_write(wfilp, buf, rbytes, &wfilp->f_pos);
			if (wbytes < 0) {
				/*failed midway do not consider as error*/
				success_files = fcounter;
				flag = 1;
				break;
			}
			success_bytes += wbytes;
		} while (rbytes >= len);
		set_fs(oldfs);
               /* 
                * Break the for loop if read/write failed mid-way
		*/
		if (flag) 
			break;
	}
//#endif
}
	oldfs = get_fs();
	set_fs(KERNEL_DS);

        /*Calculate total size of input files */
        for (fcounter = 0; fcounter < current_process->infile_count; ++fcounter) {
		vfs_stat(current_process->infiles[fcounter], &stat);
		totalsize += stat.size;
        }
	set_fs(oldfs);

	/*
	 *Depending on special flags 
	 *returning desired value
         */
	if ((current_process->flags == 0x01) || (current_process->flags == 0x05)) {
		retval = success_files;
	} else if ((current_process->flags == 0x02) || (current_process->flags == 0x06)) {
		retval = (success_bytes * 100) / totalsize;
	} else {
		retval = success_bytes;
	}

	/*Handling memory leaks */

out:		if (wfilp != NULL)	
 			filp_close(wfilp,NULL);
		if (cflag == 1) {
			if ((current_process->oflags & O_CREAT) == O_CREAT) {
				vfs_unlink(w_dir, w_dentry);
			}
		}
out_closeoutput:
                for (fcounter = 0; fcounter < m; ++fcounter)
                        filp_close(rfilp[fcounter], NULL);
		return retval;

}

/**
 * validation  -       This function validates the input arguments passed
 *		       from user and returns respective errors in case
 *		       of exceptions.
 * @current_process:   variable containing all the arguments in form of 
 *                     struct process_details type. 
 */
static int validation(struct process_details *current_process)
{
	int retval = 1;
	int n = 0;
	int fcounter;
	int m = current_process->infile_count;
	struct file *rfilp[current_process->infile_count];
	struct file *wfilp;

	/*check if outfile is NULL or not*/
	if (current_process->outfile == NULL) {
		printk(KERN_ERR "\n Error : outfile passed as NULL");
		retval = -EINVAL;
		goto out;
	}

	/*Input files missing check */
	if (current_process->infile_count == 0) {
		printk(KERN_ERR "sys_xconcat: Input file(s) missing \n");
		retval = -EINVAL;
		goto out;
	}
	/*Input files maximum limit check */
	if (current_process->infile_count > 10) {
                printk(KERN_ERR "sys_xconcat: Input files should not exceed 10\n");
                retval = -E2BIG;
                goto out;
        }


	/*Special flags validation */
	if ((current_process->flags == 0x07) | (current_process->flags == 0x03)) {
		printk(KERN_ERR "sys_xcrncat: Bad combination of special flags\n");
		retval = -EINVAL;
		goto out;
	}

	current_process->outfile = getname((char *) current_process->outfile);

	if (IS_ERR(current_process->outfile)) {
		retval = -EFAULT;
		printk(KERN_ERR "\n Error: Outfile couldn't be copied to kernel space");
		goto out;
	}

	for (fcounter = 0; fcounter < current_process->infile_count; ++fcounter) {
		current_process->infiles[fcounter] = getname(
				(char *) current_process->infiles[fcounter]);

		if (IS_ERR(current_process->infiles[fcounter])) {
			retval = -EFAULT;
			printk(KERN_ERR "\n Error: Input file couldn't be copied to kernel space");
			/*Track the input files that are copied successfully so far*/
			n = fcounter;
			goto out_putname;
		}
	}

	/*
	 * Input and Output files should not be same.For this if the
	 * access mode is 0_TRUNC to avoid deleting the content of
	 * input files open file in different mode and later on
	 * reopen if actual mode if required.
	 */
	if (current_process->oflags & O_TRUNC) {
		wfilp = filp_open(current_process->outfile, 
			current_process->oflags ^ O_TRUNC,current_process->mode);
	} else {
		wfilp = filp_open(current_process->outfile,
				current_process->oflags,current_process->mode);
	}

	if (!wfilp || IS_ERR(wfilp)) {
		retval = (int) PTR_ERR(wfilp);
		printk(KERN_ERR "\n Error : Opening output file ");
		goto out_putname;
	}

	if (!wfilp->f_op->write) {
		retval = (int) PTR_ERR(wfilp);
		printk(KERN_ERR "\n Error: File doesn't allow writes");
		goto out_putname;
	}

	/*
	 * open all the files and validate each of them
	 */
	for (fcounter = 0; fcounter < current_process->infile_count;
							 ++fcounter) {

		rfilp[fcounter] = filp_open(current_process->infiles[fcounter],
				O_RDONLY, 0);

		if (!rfilp[fcounter] || IS_ERR(rfilp[fcounter])) {
			printk(KERN_ERR "\n Error: Opening input files ");
			retval = (int) PTR_ERR(rfilp[fcounter]);
			m = fcounter;
			goto out_closeoutput;

		}

		if (!rfilp[fcounter]->f_op->read) {
			retval = (int) PTR_ERR(rfilp[fcounter]);
			printk(KERN_ERR "\n Error: File doesn't allow reads.");
			m = fcounter;
			goto out_closeoutput;
}

		 /*
		  * check whether both  input and output files are same.
		  * in case of failure always track down the number of
		  * files opened successfully so far.
		  */ 
		if (wfilp->f_dentry->d_inode == 
					rfilp[fcounter]->f_dentry->d_inode) {
			retval = -EINVAL;
			printk(KERN_ERR "\n Error:Input and Output files should not be same");
			m = fcounter;
			goto out_closeoutput;

		}
	}

/*Handling memory leaks */

out_closeoutput:
		filp_close(wfilp, NULL);
		for (fcounter = 0; fcounter < m; ++fcounter)
			filp_close(rfilp[fcounter], NULL);
out_putname:
		 putname(current_process->outfile);
		for (fcounter = 0; fcounter < n; ++fcounter)
			putname(current_process->infiles[fcounter]);
out:
		return retval;

}
static int __init init_sys_xconcat(void)
{
	printk(KERN_INFO "installed new sys_xconcat module\n");
	if (sysptr == NULL)
		sysptr = xconcat;
	return 0;
}
static void __exit exit_sys_xconcat(void)
{
	if (sysptr != NULL)
		sysptr = NULL;
	printk(KERN_INFO "removed sys_xconcat module\n");
}
module_init(init_sys_xconcat);
module_exit(exit_sys_xconcat);
MODULE_LICENSE("GPL");
