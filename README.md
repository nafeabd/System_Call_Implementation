System_Call_Implementation
==========================

Implementation of User defined System call which simulates the behavor of Existing Cat System call with many user options
READDME


What does this file contains?

This file contains implementation details of this program for home work 1 assignment.Mainly the strategies that are involved
to implement this code are detailed here.Some of the underlined assumptions that were made in development of this program
are also mentioned here.

Purpose:

The purpose of this home work 1 project to implement a system call by making changes in the kernel which concatenates
user given input files into destination target file . Apart from this program implements some more additional functionality like
by restructuring standard outputs in different ways as per user specified inputted flags.

User land module:

This module acts as an interface between user and kernel implementation of system call.
It takes all the inputs that are inputted from user through command prompt and packs all the inputs in a special structure 
variable which has below details

	1)Output file 
	2)List of Input files
	3)No of input files
	4)Open flags to change the behavior of system call
	5)Default permissions for newly created output file
	6)Special flags details to change the behavior of system call.

It packs all the above arguments and calls the actual system call (whose implementations is at kernel level) and finally
prints the status of the program , i.e success full bytes/success full files/percentage (as per user arguments) and in case of
failure error number and error descriptions are printed on console

Main Module:

This module contains actual implementation of system call.The system call implementation is done
at kernel level using help of some helper functions that are available.When the input data is passed to this module
(program) before starting concatenation operations some initial validations are being performed.

Validations:

	1)Arguments length passed should be valid one . In our case this should be the size of structure that we defined
	2) Checking the validity of User address space using copy_from_user function.
	3)Output file should not be null
	4)Missing input files(Input file count should not be zero)
	5)Maximum number of input files that are allowed for this program is 10 . If exceeds throws error.
	6)Validation against bad combination of special flags . 
	7)Input files and output files should not be same
	8)User don't have permissions to read/write the file 
	9)Failures during opening/reading/writing the files
	10)Invalid mode (This checking is done at user level program . If user inputs invalid mode which is non octal number , error is displayed
	      and program terminates.)

Assumptions:

Some basic default conditions are assumed in certain cases . Below describes them .
	1)When no mode is specified , by default this program considers as 755(owner has read,write,execute but others have only read/write)
	2)When a file exists and user specified none of the flags , by default append is performed
	3)When a file exists and user specifies create flag , then over-write takes place in the content of existing file
	4)Invalid mode is one which is non-octal number and in this case function strtol returns zero .

Base assignment mode

 In general mode,input files are first opened and if in case any errors , then respective error is thrown . Same with opening output file.
In this base mode , while reading/writing files if any operation fails mid way , then respective bytes/files/percentage read so far successfully
is returned.

Atomic mode

In this mode , if operation fails mid way then roll back is performed and original file contents are preserved . Atomic mode code is wrapped in 
EXTRA_CREDIT using ifdef , and by default this macro is defined.Ideally to run in atomic code this has to be turned on (macro should be defined in file)
and in case if atomic mode is not defined and user tries to input using this mode respective error is displayed.

Strategy employed for atomic mode:

Every time a temporary file is taken and all inputs files are written into it respectively.
If any operation is failed mid way then temporary file in unlinked. If successful original file is unlinked and temporary file is renamed.
 While doing so if append mode is given so save contents of original file , before writing input file contents
into temporary file , original file contents are copied into it first . 
Always respective measures are taken to prevent original file content in any case if failure occurs.
