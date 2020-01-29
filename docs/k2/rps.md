# K2 Output (RPS)

## Transcript

```
     1	Hello from the choochoos kernel!
     2	random seed (>= 0): 2
     3	pause after each game (y/n)? n
     4	num players (0-32): 3
     5	player 1 priority  (default 1): 1
     6	player 1 num games (default 3): 3
     7	player 2 priority  (default 1): 2
     8	player 2 num games (default 3): 5
     9	player 3 priority  (default 1): 3
    10	player 3 num games (default 3): 3
    11	[Client 3] I want to play 3 games. Sending signup...
    12	[Client 4] I want to play 5 games. Sending signup...
    13	[RPSServer] matching tids 4 and 3
    14	[Client 4] received signup ack
    15	[Client 4] I want to play 5 more games. Sending scissors...
    16	[Client 5] I want to play 3 games. Sending signup...
    17	[Client 3] received signup ack
    18	[Client 3] I want to play 3 more games. Sending paper...
    19	[Client 4] I won!
    20	[Client 4] I want to play 4 more games. Sending paper...
    21	[Client 3] I lost :(
    22	[Client 3] I want to play 2 more games. Sending rock...
    23	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    24	[Client 4] I won!
    25	[Client 4] I want to play 3 more games. Sending rock...
    26	[Client 3] I lost :(
    27	[Client 3] I want to play 1 more game. Sending rock...
    28	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    29	[Client 4] it's a draw
    30	[Client 4] I want to play 2 more games. Sending rock...
    31	[Client 3] it's a draw
    32	[Client 3] sending quit
    33	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    34	[RPSServer] tid 3 quit, but tid 5 is waiting. Matching tids 4 and 5
    35	[Client 5] received signup ack
    36	[Client 5] I want to play 3 more games. Sending scissors...
    37	[Client 3] exiting
    38	[Client 4] I won!
    39	[Client 4] I want to play 1 more game. Sending scissors...
    40	[Client 5] I lost :(
    41	[Client 5] I want to play 2 more games. Sending rock...
    42	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    43	[Client 4] I lost :(
    44	[Client 4] sending quit
    45	[Client 5] I won!
    46	[Client 5] I want to play 1 more game. Sending paper...
    47	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    48	[RPSServer] tid 4 quit, but no players are waiting.
    49	[Client 4] exiting
    50	[Client 5] other player quit! I guess I'll go home :(
    51	[Client 5] exiting
    52	Goodbye from choochoos kernel!
```

## Explanation
