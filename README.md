# UnixChat

Chat Application for linux written in C

## How to build
  ```
  make
  ```

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