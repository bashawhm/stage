# Stage: A Unix command shell

Stage is a unix style command shell for my CS444 (Operating Systems) class.
The parser was provided by Professor Jeanna Matthews. 

# Features

Stage supports several things...

| Features         | Example              |
| ---------------- |:--------------------:|
| Process Creation | ./foo.sh | /bin/cat  |
| IO Redirection   | ./foo.sh > bar.txt   |
| Background Jobs  | ./foo.sh &           |
| History          | history              |
| History Recall   | !3                   |
| List builtins    | help                 |

# Known Bugs

The only known bug is that if you kill a background process and imediatly
try to exit, the shell will not yet have checked the background process list
and will still think there are background processes alive.

# Unknown Bugs

Likely.