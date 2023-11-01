1. [ ] job needs to be at pipeline-level, not command-level
2. [x] $ export user3="$USER person" should work but it doesn't - could just make export combine args
3. go over project requirements and test all others
4. memory leaks, memory leaks, memory leaks
5. [x] maybe let tokens have flags like tok.is_number or tok.variable_word
6. [x] funky stuff happens with strings as args to echo (and probably other functions)
7. [x] need to make variables expand out into one single token
8. [x] make variable expansion happen after glob expansion
  1. [x] expanded var becomes part of enclosing double-quote string
  2. [x] entire expanded var is its own single token
9. [x] do i really need to make a pratt parser to handle precedence
10. [x] tilde expansion
  1. [ ] tilde expansion when tilde is first char
11. [x] comments
12. [x] give up on arbitrary fd redirection with 2> and 2>&1, who does that anyways lol
13. [x] i guess im making a hash table now
14. [ ] SIGINT stops async processes now; need to set ignorers?

quash requirements
- comments
- operations
  - pipes
  - redirect stdout to file
  - redirect stdout to file, apppending
  - redirect stdin to file
- builtins
  - echo
  - export
  - cd
  - pwd
  - quit & exit
  - jobs
  - kill

tiers
- tier 0
  - [x] project compiles
- tier 1
  - [x] commands without args
  - [x] pwd
  - [x] echo
- tier 2
  - [x] commands with args
  - [x] echo, cd, export
  - [x] environment variables
- tier 3
  - [x] jobs, kill
  - [x] piping between commands
  - [x] redirect stdin
  - [x] redirect stdout
  - [x] background processes
    - [x] job completion notification
- tier 4 (extra credit)
  - [x] mixing pipes and redirects
  - [x] pipes and redirects work with built-in commands
  - [x] append redirection

# features

- essential
  - commands without arguments
  - commands with arguments
  - variable assignment and variable expansion in commands
  - `echo`, `export`, `cd`, `pwd`, `quit`, `exit`, 
  - `jobs`, `kill`
  - `>` redirect
  - `<` redirect
  - pipes
- extra credit
  - conditional AND (`&&`) and OR (`||`)
  - pipes, redirects, and conditionals eval in proper order
  - pipes and redirects work with built-in commands
  - `<>` redirect (redirect stdout and stdin to/from file)
  - `>>` redirect (redirect stdout to file, appending)
  - `>&` redirect (redirect stderr to file)
  - `>>&` redirect (redirect stderr to file, appending)
  - GNU readline & history
