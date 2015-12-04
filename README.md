# CPSC 3600 Final Project

-------------------------------------------------------------------------------  
FILE CONTENTS
-------------------------------------------------------------------------------
* GROUP MEMBERS
* PROJECT DESCRIPTION
* OTHER COMMENTS
* KNOWN PROBLEMS
* DESIGN

-------------------------------------------------------------------------------  
I. GROUP MEMBERS
-------------------------------------------------------------------------------
* Group Members: Josh Gates, Grace Glenn, Ben Green, Andrew Zhang
* Project Name: Robot Proxy Client and Server
* Due Date: Friday, December 4th, 2015, 2:30 pm

-------------------------------------------------------------------------------  
II. PROJECT DESCRIPTION
-------------------------------------------------------------------------------
This project is made up of a series of programs that enables the user to 
control a robot remotely. This control is provided by running a robot client
that communicates with a server sending requests and commands to the robot.

The first connection, between the client and server, occurs over a single UDP
connection. The second, between the server and robot, occurs over multiple
TCP connections. The server de/multiplexes the raw information of several
aspects of the robot's information -- such as GPS, dGPS, laser, and image data,
and sends it to the client based on what it has requested.

The client controls the robot explicitly by asking the server to move, turn, or
stop the robot. Commands are ordered to make a the robot draw two shapes: a polygon
of sides N with length L, and a second polygon of sides N - 1 and length L.

* Source Files:

    FILE NAME           FILE DESCRIPTION
    
    client.c            requests information about robot from server
    Makefile            Makefile to compile program
    run_client          bash script to run client with default settings
    run_server          bash script to run server with default settings
    server.c            runs a tcp client and udp server
    udp_protocol.c      provides functionality for parsing and creating udp datagrams
    udp_protocol.c      udp_protocol header, furnishes robot commands and header info
    utility.c           provides functionality for creating and manipulating buffers
    utility.h           utility header

-------------------------------------------------------------------------------  
III. OTHER COMMENTS
-------------------------------------------------------------------------------
* Compilation:

The program should be compiled using the Makefile with the following terminal
command:
            $ make 

To remove all object files:

            $ make clean

* Running Programs:

The following terminal lines should be used to run the programs after compilation:

        Machine 1
            $ ./server -h <hostname-of-robot> -i <robot-id> -n <robot-number> -p <port>
        
            -h <hostname-of-robot>: Address/Hostname of the robot to communicate with
            -i <robot-id>: ID of robot
            -n <robot-number>: Number of robot; used to get pictures
            -p <port>: Port that the server will listen on for connections

        Machine 2
            $ ./client -h <hostname-of-server> -p <port> -n <number> -l <length-of-sides>
    
            -h <hostname-of-server>: Address/Hostname of the proxy server
            -p <port>: Port that the proxy server is listening on
            -n <number-of-sides>: A number between 4 and 8 (inclusively) defining the 
            number of sides
            -l <length-of-sides>: A number that defines the length of the sides 
            
* Default Usage: 

To run the program in default mode:

    Machine 1
            $ bash run_server
    
    Machine 2
            $ bash run_client

-------------------------------------------------------------------------------  
IV. KNOWN PROBLEMS
-------------------------------------------------------------------------------
* Interactive Mode 

In interactive mode (running $ bash run_client -i for example), there is not option to 
run connect, instead it's assumed. This may be useful implementing in a future version
to allow for various forms of tests or other features.

* Error Messages from Server to Client

Server prints error messages to stdout but does not send appropriate error messages
to client all the time. Most of these cases are confined to sitations where the server
ignores an invalid or redundant request from the client.

* Buffer Edge Cases?

We've implemented functional buffers in this project and have fixed errors regarding them
along the way, but have yet to test for all edge cases. One major example is when we were
re-sizing buffers in append_buffer(). When we rezied buffer through append_buffer, we 
would only double the amount od data the buffer could hold. This caused issues when 
appending large amounts of data into small buffers. This was resolved by resizing the 
buffer via a loop to ensure enough space was given.

* Robot Image Port Not Functional

We have yet to fully troubleshoot all the reasons the robot's image port hasn't
succesffully worked with our server on these ocassions.

-------------------------------------------------------------------------------  
V. DESIGN
-------------------------------------------------------------------------------
* Server

Long timeouts between client requests to allow for the client to sleep up to 60 seconds between commands.This is useful for long moves and turns.

A statefull design that controls what the server expects and processes.
A disconnected state when the server is not bound to a single client. In this state the server waits for a connect request from the client, ignoring all other requests Upon receiving a connect request the server moves into a second state.

This second state has the server waiting for an ack from the client it received a connect from. Any other messages are ignored. Upon receiving the ack the server moves into its final state.

The final state processes the full range of messages, less connect, and relays the clients' wishes to the robot. This state persists until either a quit message from the client is received or the client doesn't communicate with the server for a long time (currently 60 seconds).

* Client

Boots up and connects to the server. After confirming the connection with the server, the client begins sending commands that will result in the robot moving in a shape based on the number of sides given to the client on startup.

Also has an interactive mode which allows real time interaction with the robot.

The Client has to deal a major aspect of UDP messaging in that it is possible for 
datagrams to arrive out of order. In this spirit of the header, the client takes
the byte offset of each datagram and uses this to position the payload in the 
pre-eminent total message. 

The client also needed to provide a way of leveraging the robot commands to turn
the robot in two specified polygons with length L and sides N and N - 1. This was done
by using our function for proxy requests, get_thing, and using the metrics outlined
in the spec for lienar velocity and angular velocity to have the robot continually 
execute moves and turns. 

This hides a great deal of the complexity in the function move_robot, allowing
the user to understand what is going on without reading every function to boot.
