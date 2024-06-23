# Vertext Text Editor

Vertext is a simple, lightweight text editor written from scratch in C. This project is inspired by the Kilo text editor (see Acknowledgements for more details) and is for educational purpose only.

# Installation

The installation instructions are for Windows Operating System only and assumes that you already have [Cygwin](https://www.cygwin.com/) along with its GCC and Make components (used for compiling C program files and running Makefile respectively) installed.

To install and run Vertext, follow these steps:  

1. Clone the repository:  
```sh
git clone https://github.com/im-varun/vertext.git
```  

2. Navigate to the project directory:  
```sh
cd vertext
```  

3. Compile the source code:  
```sh
make
```  

# Usage

The text editor uses standard editing keys. Some of the basic commands with their shortcuts are:  
1. Open vertext: `./vertext [OPTIONAL - FILENAME]`  
2. Save a file: `Ctrl + S` (file will be saved in the current directory)  
3. Quit the editor when the file is unmodified: `Ctrl + Q`  
4. Quit the editor when the file is modified: `Ctrl + Q` 3 times

# Acknowledgements
- [Kilo Text Editor](https://github.com/antirez/kilo) by [antirez](https://github.com/antirez)
- [Kilo Text Editor Tutorial](https://viewsourcecode.org/snaptoken/kilo/) by [snaptoken](https://github.com/snaptoken)