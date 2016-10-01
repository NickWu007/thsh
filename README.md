## thsh shell ##
This is an implementation of the unix shell, adding extra features like debugging mode, variable support, scripting, and more. 

DO NOT use this code for your university projects.

### Running Command ###
You will be able to run you usual bash commands:

~~~
thsh> pwd
/home
thsh> ls
# shows files in /home
thsh> cd /tmp
thsh> pwd
/tmp
thsh> ls
# shows files in /tmp
~~~

### Debugging Mode ###
If you start thsh with `-d`, it should display debugging info on stderr:

 * every command executed should say "RUNNING: cmd", where cmd is replaced with the text of the command.
 * when command ends you should say "ENDED: "cmd" (ret=%d)" and show it's exit status.
 * any other debugging info.

### Variable Support ###
You will be able to use `set` and `echo` to add variables and output them.

~~~
[/home] thsh> echo $PATH
/bin:/usr/bin
[/home] thsh> set PATH=/bin:/usr/bin:.
[/home] thsh> echo $PATH
/bin:/usr/bin:.
[/home] thsh> ls
foo.c Makefile
[/home]thsh> echo $?
0
[/home]thsh> ls /blah
/blah: no such file or directory
[/home]thsh> echo $?
1
~~~

The special variable `$?` stores the return code of the last command.

### Input/Output Redirection ###
Like in bash you will be able to use `|` to pipe commands, and `<` and `>` to direct input/output to files.

~~~
thsh> ls -l >newfile
thsh> cat < newfile
....
thsh> somecommand 2>err.log
thsh> ls | grep .txt | wc -l
4
~~~

### Scripting ###
You can start the thsh shell in non-interactive mode by giving a script as argument:

~~~
$ thsh foo.sh
~~~

Or in thsh shell, you can run a script by:

~~~
thsh> cat foo.sh
#!thsh
ls -l
echo hello world
thsh> chmod u+x foo.sh
thsh> ./foo.sh
# Output from executing foo.sh
~~~ 

### Disclaimer ###
This is only for people who want to implement their won shell to get some inspiration. If you are working on a similar university project, please do not use or copy and change this code.


