# UnixChat

Chat Application for linux written in C

## How to build
  ```
  make
  ```
Note that this project uses libncurses5-dev library. Makefile installs the library using sudo and therefore, you need to type your password. :)
## How to run
Run the server first
  ```
  ./bin/server
  ```
and then the client
  ```
  ./bin/client localhost
  ```

## Chat commands

| Command       | Parameter             |                                     |
| ------------- | --------------------- | ----------------------------------- |
| /PM           | [name] [message]      | Sends private message to [name]     |
| /whosonline   |                       | Show online users                   |
| Ctrl + C      |                       | Disconnects client/closes server    |
