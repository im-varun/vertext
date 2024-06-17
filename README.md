# Vertext Text Editor

Vertext is a simple, lightweight text editor written from scratch in C. This project is inspired by the Kilo text editor (see Acknowledgements for more details) and is for educational purpose only.

# Installation

The installation instructions are for Windows Operating System only and assume that you already have [Cygwin](https://www.cygwin.com/) along with its GCC and Make components (used for compiling C program files and running Makefile respectively) installed on your PC.

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

4. Run the text editor:  
To open a blank file, run `./vertext`  
To open an existing file (name=filename), run `./vertext filename`

# Usage

Once you have the text editor running, you can start editing text files. The text editor uses standard editing keys. Some of the basic commands with their shortcuts are:  
1. Open a file: `./vertext filename`  
2. Save a file: `Ctrl + S`  
3. Quit the editor when the file is unmodified: `Ctrl + Q`  
4. Quit the editor when the file is modified: `Ctrl + Q` 3 times

# Acknowledgements
- [Kilo Text Editor](https://github.com/antirez/kilo) by [antirez](https://github.com/antirez)
- [Kilo Text Editor Tutorial](https://viewsourcecode.org/snaptoken/kilo/) by [snaptoken](https://github.com/snaptoken)

# License
Vertext © 2024 by Varun Mulchandani is licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).