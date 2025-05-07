# CSC345_Project_4
Project_4


CSC 345 – Project #4: Network Programming
Due: May 9, 2025, before midnight
Objective:
Design and implement a multi-user chatting program using socket library on Linux.
Due Date:
May 9, 2025, 11:55pm
Project Details:
In this project, you will implement a command line-based chatting program using socket library.
We provide a skeleton program (chat_server/client, chat_server_full/client_full) to demonstrate
basic behavior of the chatting program. Please note that the programs are not in a complete form,
and may have some bugs. To test the programs, download and extract the zip file, and run make
to compile. Open two terminals, and in one terminal, type
$ ./chat_server OR $ ./chat_server_full 
and in the other terminal, type
$ ./chat_client 127.0.0.1 OR $ ./chat_client_full 127.0.0.1
and see how they work. The difference between chat_server/client and
chat_server_full/client_full is that the latter server can accept multiple clients. Thus, you can
open yet another terminal, and try to connect to the server_full with another instance of
client_full. Then, the server_full will route message from one client to the other (actually, it is
supposed to broadcast the message to all connected clients).
You can use these programs as your starting point. Create your own program, named
[main_server] and [main_client] and submit only yours. Below list explains the required
features of the chat program. Note that not all edge cases are explicitly described, and some
natural, obvious features one would expect from a chat program will be implicitly tested. For
example, your chat client program must connect and disconnect from the server smoothly,
without “hic-ups” or random seg faults. The server must properly process the clients’ connect or
disconnect requests. Normally, clients can request disconnect by entering an empty message.
For this assignment, it is extremely important that you clearly state upfront (on the top of
the first page of your report.pdf), up to which check point you implement. Because each
check point may have a different user interface and execution method (e.g., command line
argument), if you do not specify which check point you want me to check clearly, it is possible
that the grading is based on different criteria. Also, you must attempt check points in order. In
other words, if you have not completed all requirements in Check Point 2, you must not attempt
Check Point 3 items. If you do, items in Check Point 3 will not be graded and the grading will
use the user interface and command line argument for Check Point 2.
2
Program Requirements (100 pts):
• (10 points) main_server can display the up-to-date list of connected clients. When a new
client connects or any client disconnects, the up-to-date list will be printed in the server-
side terminal. It should work exactly in the following fashion:
[in server terminal]
$ ./main_server
[in client terminal]
$ ./main_client IP-address-of-server
• (10 points) main_server accepts multiple main_client connections and broadcast
messages from one client to all connected clients properly.
• (10 points) main_client can specify user name when connecting to the server. The server
will maintain the user names and broadcast the names to clients properly. The client
program will run as follows (Note that in this example, Bob was already in the room):
[in client terminal]
$ ./main_client IP-address-of-server
Type your user name: Alice
Alice (IP-address-of-Alice) joined the chat room!
hello
[Bob (IP-address-of-Bob)] Hi, Alice!
Great, how about you?
...
• (10 points) When a user connects, all connected clients should be notified with a message
like the example above. When a user disconnects, if exists, remaining users in the
chatroom should be notified about the user’s leaving. For example, “Alice (IP-address)
left the room!”
• (10 points) Use a random, unique color for each client in a room. It is okay if clients are
using different sets of colors for the same room information. However, within the same
client, individual users must be assigned unique colors to distinguish each other. Below
table shows an example output of three clients (IP addresses are omitted in the sample):
Client 1 (Alice) Client 2 (Bob) Client 3 (Claire)
Hi!
[Bob] How are you?
Awesome!
[Claire] Very well!
OS is fun to learn.
[Bob] I agree!
[Alice] Hi!
How are you?
[Alice] Awesome!
[Claire] Very well!
[Alice] OS is fun to learn.
I agree!
[Alice] Hi!
[Bob] How are you?
Very well!
[Alice] Awesome!
[Alice] OS is fun to learn.
[Bob] I agree!
*** Check Point 1: Make sure your program smoothly runs without any hic-ups for
the five requirements above before trying the harder requirement below. ***

• (15 points) main_server allows multiple chatting rooms running simultaneously. For this
purpose, the client behavior will be slightly changed. Client can request whether it will
open a new chatting room or join an existing room. If a client wants to open a new chat
room, then it should connect to the server as
[in client terminal]
$ ./main_client IP-address-of-server new
If the connection was successful, then the client will be assigned a room number from the
server and display it on the screen, e.g.
[in client terminal]
$ ./main_client IP-address-of-server new
Connected to IP-address-of-server with new room number XXX
If the client wants to connect to an existing room, then it can use a command as
[in client terminal]
$ ./main_client IP-address-of-server XXX
where XXX is the pre-existing room number. You can assume that all rooms are
numbered.
The main_server is responsible for maintaining the rooms and clients therein properly. If
a main_client requests for non-existing room number, it should properly block the
request. You can optionally set the maximum number of rooms to be managed by the
server, but it should be at least 3.
*** Check Point 2: Make sure your program smoothly runs without any hic-ups for
the requirement above before trying the harder requirement below. ***
• (20 points) In case the client neither specify the room number nor used new keyword, it
will retrieve the list of the rooms available currently and choose from the list as below:
[in client terminal]
$ ./main_client IP-address-of-server
Server says following options are available:
Room 1: 2 people
Room 2: 1 person
...
Room 10: 5 people
Choose the room number or type [new] to create a new room: _
If no room is available, the server should automatically create and join the user to the
new room (thus implicitly using new command from the above).
*** Check Point 3: Make sure your program smoothly runs without any hic-ups for
the requirement above before trying the harder requirement below. ***

• (15 points) Allow file transfer. A client can send a local file to a remote client with a
special command [SEND]. The syntax is like below
[in client terminal]
[this-client-IP-or-user-name]: SEND receiving-user-name file-name
The receiving client should be notified of the transfer and agree on receiving the file, by
Y/N question. When the receiving client types, “Y”, then the transmission starts. You can
assume that the file being sent is always in the same directory as the sender chat client
program. There is no assumption of file type. A binary file should be properly transmitted,
if requested. Keep in mind that the devils are in the details. For example:
o If you test two clients running on the same machine, running from the same directory,
you may accidentally overwrite the source file. This should not happen. It is best to
test by running two clients running from different directories so that you can confirm
that the file from one directory has been copied to the other.
o Clients connected to the same server room should not know about the file transfer if
they are not the sender/receiver of the file. Also, a user should not be able to send a
message or a file to a user who is not in the same chat room. This is a serious security
vulnerability that was often found in early commercial chat applications.
o Be aware of the potential deadlock. There is also a potential race condition depending
on your implementation. For example, if sender or receiver drops the connection
unexpectedly in the middle of transmission, the transmission should be terminated
gracefully on the other end.
o It is possible that non-file-transfer-involved user may send a message to the server
while the file transmission is ongoing! Their communication should not be blocked
by the file transfer.
o It is possible multiple clients may try to initiate independent transfers. Hence, proper
and accurate implementation of this is to create a separate asynchronous thread that
will handle file transfers, and notify the chatting thread when the transmission is
completed.
*** Check Point 4: If you completed all requirements, indicate that you reached the
Check Point 4 in the report.pdf. Do not attempt the extra item below unless you
reached this point without any hic-ups. ***
• (EXTRA; 5 points) Using libssh we learned in class, re-implement the chat application to
use SSH tunneling. If you implement this in Direct Port Forwarding, the chat server will
be the remote end point, and the chat client will connect to the server via SSH client-
server connection established via libssh. A related tutorial can be found in the link:
https://api.libssh.org/stable/libssh_tutor_forwarding.html
Extra credit amount will be determined based on the maturity of the implementation.
Trivial attempt will not receive points.
*** Check Point EX: If you completed the extra requirement, indicate that you
reached the Check Point EX in the report.pdf. ***

Formatting Requirements
• Comment the part of code you made changes.
• In a separate file (report.pdf), clearly describe which requirements you implemented.
Clearly indicate that, up to which checkpoint you implemented successfully (or not).
• In another separate file (discussion.pdf), attach your group discussion log including
date/time.
• Put all your files into the zip file.
./project4.zip // your zip submission
|- report.pdf // your project report
|- discussion.pdf // your group discussion log (same format as Project 2)
|- main_server.c // your server program
|- main_client.c // your client program
|- Makefile // makefile (REQUIRED)
and put your implementations inside. NEVER include the sample codes from the lab!
• Your Makefile should compile each source as a separate executable program. For
example, main_server.c should compile to make main_server and main_client.c must
compile to make main_client. Take extra caution about the naming convention. This
includes the report.pdf and discussion.pdf. Do NOT use random file names for the files
listed above. They must use the names as shown.
• Your main_server.c and main_client.c must NOT depend on ANY other file(s) and
they must compile standalone, without depending on any other .h, .c, .a, or
ANYTHING. NEVER ADD ANY OTHER SOURCE OR HEADER FILES.
EVERYTHING SHOULD BE INSIDE main_server.c or main_client.c. The only
allowed exception is the C runtime library pre-installed on the provided VM.
• When extracted, your submission should NOT create a directory or subdirectory. The
files inside your zip file should be extracted in the same directory where the zip file is.
Use Linux terminal’s unzip command to see if your zip file is good.
• Any formatting violations will receive at least 30% penalty of the maximum grade.
A 100% penalty (zero) is possible if formatting violations happen repeatedly.
What to turn in
• Zip the folder containing your source file(s) and Makefile, then submit it through Canvas
by the deadline. There will be a penalty for not providing any working Makefile.
Advise
• Command line user interface syntax at each check point is different from the others.
Hence, it is a good idea to make a backup of your source code at each check point
before you try the next requirement so that you could come back if you got into a trouble.
• DO NOT CHANGE THE USER INTERFACE PROMPT TEXT (e.g., “Choose the
room number or type [new] to create a new room”) OF THE PROJECT. FOLLOW
THE DESCRIPTION AS-IS.

Special Rule (Spring 2025)
For this assignment ONLY, you are allowed to get help from AI-based tools in following ways:
• Write prompts to ask general structural / organization guideline, general questions about
socket library usage and examples
• Upload your work-in-progress code, including the provided skeleton code, to an AI code
assistant user interface (e.g., chatbot-style AI service) to get help in debugging and
refactoring
• Use an IDE built-in or plug-in based AI assistant services (e.g., CoPilot for VS Code)
• Properly acknowledge the parts of your code that you used AI tools, e.g., using
comments.
In your report.pdf, you must provide a short essay of the following:
• Your ways of AI tool usage for your project, IN DETAIL (including prompts, responses,
workflow, hallucination handling, etc.). I want everything: for what reason you consulted
the AI tool, you used in which ways, and how it worked out or not worked, etc.
• Your reflection / thoughts / opinions on the usage of the AI tools for the project
• The length of the essay depends on how you used the tool, so there is no limit. For the
format, it should be consistent with the other parts of your report.pdf.
If you don't provide the essay, I would assume that you did not use AI tools. In that case, if your
submission was found to have used AI, then you will be reported for academic integrity violation
of Misrepresentation.
