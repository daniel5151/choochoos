# K2 Output (RPS)

## Transcript

```
     1	Hello from the choochoos kernel!
     2	random seed (>= 0): 2
     3	pause after each game (y/n)? y
     4	num players (0-32): 3
     5	player 1 priority  (default 1): 1
     6	player 1 num games (default 3): 3
     7	player 2 priority  (default 1): 2
     8	player 2 num games (default 3): 5
     9	player 3 priority  (default 1): 3
    10	player 3 num games (default 3): 3
    11	[Client tid=3 id=?] waiting for player config
    12	[Client tid=4 id=?] waiting for player config
    13	[Client tid=5 id=?] waiting for player config
    14	[Client tid=5 id=?] received player config (num_games=3, id=3)
    15	[Client tid=5 id=3] querying nameserver for 'RPSServer'
    16	[Client tid=4 id=?] received player config (num_games=5, id=2)
    17	[Client tid=4 id=2] querying nameserver for 'RPSServer'
    18	[Client tid=3 id=?] received player config (num_games=3, id=1)
    19	[Client tid=3 id=1] querying nameserver for 'RPSServer'
    20	[RPSServer] accepting signups...
    21	[Client tid=5 id=3] received reply from nameserver: RPSServer=2
    22	[Client tid=5 id=3] I want to play 3 games. Sending signup...
    23	[Client tid=4 id=2] received reply from nameserver: RPSServer=2
    24	[Client tid=4 id=2] I want to play 5 games. Sending signup...
    25	[RPSServer] matching tids 4 and 5
    26	[Client tid=4 id=2] received signup ack
    27	[Client tid=4 id=2] I want to play 5 more games. Sending scissors...
    28	[Client tid=3 id=1] received reply from nameserver: RPSServer=2
    29	[Client tid=3 id=1] I want to play 3 games. Sending signup...
    30	[Client tid=5 id=3] received signup ack
    31	[Client tid=5 id=3] I want to play 3 more games. Sending paper...
    32	[Client tid=4 id=2] I won!
    33	[Client tid=4 id=2] I want to play 4 more games. Sending paper...
    34	[Client tid=5 id=3] I lost :(
    35	[Client tid=5 id=3] I want to play 2 more games. Sending rock...
    36	~~~~~~~~~ press any key to continue ~~~~~~~~~
    37	[Client tid=4 id=2] I won!
    38	[Client tid=4 id=2] I want to play 3 more games. Sending rock...
    39	[Client tid=5 id=3] I lost :(
    40	[Client tid=5 id=3] I want to play 1 more game. Sending rock...
    41	~~~~~~~~~ press any key to continue ~~~~~~~~~
    42	[Client tid=4 id=2] it's a draw
    43	[Client tid=4 id=2] I want to play 2 more games. Sending rock...
    44	[Client tid=5 id=3] it's a draw
    45	[Client tid=5 id=3] sending quit
    46	~~~~~~~~~ press any key to continue ~~~~~~~~~
    47	[RPSServer] tid 5 quit, but tid 3 is waiting. Matching tids 4 and 3
    48	[Client tid=3 id=1] received signup ack
    49	[Client tid=3 id=1] I want to play 3 more games. Sending scissors...
    50	[Client tid=5 id=3] exiting
    51	[Client tid=4 id=2] I won!
    52	[Client tid=4 id=2] I want to play 1 more game. Sending scissors...
    53	[Client tid=3 id=1] I lost :(
    54	[Client tid=3 id=1] I want to play 2 more games. Sending rock...
    55	~~~~~~~~~ press any key to continue ~~~~~~~~~
    56	[Client tid=4 id=2] I lost :(
    57	[Client tid=4 id=2] sending quit
    58	[Client tid=3 id=1] I won!
    59	[Client tid=3 id=1] I want to play 1 more game. Sending paper...
    60	~~~~~~~~~ press any key to continue ~~~~~~~~~
    61	[RPSServer] tid 4 quit, but no players are waiting.
    62	[Client tid=4 id=2] exiting
    63	[Client tid=3 id=1] other player quit! I guess I'll go home :(
    64	[Client tid=3 id=1] exiting
    65	Goodbye from choochoos kernel!
```

## Explanation

```
     1	Hello from the choochoos kernel!
     2	random seed (>= 0): 2
     3	pause after each game (y/n)? y
     4	num players (0-32): 3
     5	player 1 priority  (default 1): 1
     6	player 1 num games (default 3): 3
     7	player 2 priority  (default 1): 2
     8	player 2 num games (default 3): 5
     9	player 3 priority  (default 1): 3
    10	player 3 num games (default 3): 3
```

Game setup (The user's input is included in the transcript). The random number generator is configured with a seed of 2, and we pause for input after each game is finished.  We are running 3 client tasks, with priotities (1,2,3) and wanting to play (3,5,3) games respectively.

```
    11	[Client tid=3 id=?] waiting for player config
    12	[Client tid=4 id=?] waiting for player config
    13	[Client tid=5 id=?] waiting for player config
    14	[Client tid=5 id=?] received player config (num_games=3, id=3)
    15	[Client tid=5 id=3] querying nameserver for 'RPSServer'
    16	[Client tid=4 id=?] received player config (num_games=5, id=2)
    17	[Client tid=4 id=2] querying nameserver for 'RPSServer'
    18	[Client tid=3 id=?] received player config (num_games=3, id=1)
    19	[Client tid=3 id=1] querying nameserver for 'RPSServer'
```

The client tasks wait for their configuration (sent from the main task), and then query the NameServer for "RPSServer". Note that while the clients ask for their configuration in order, the highest priority task is (Tid 5) is woken up first, and thus queries the NameServer first.

```
    20	[RPSServer] accepting signups...
    21	[Client tid=5 id=3] received reply from nameserver: RPSServer=2
    22	[Client tid=5 id=3] I want to play 3 games. Sending signup...
    23	[Client tid=4 id=2] received reply from nameserver: RPSServer=2
    24	[Client tid=4 id=2] I want to play 5 games. Sending signup...
    25	[RPSServer] matching tids 4 and 5
```

The RPSServer (priority 0) starts accepting signups. Clients 3 and 2 are the highest priority, so their signup requests are received first, and they are matched.

```
    26	[Client tid=4 id=2] received signup ack
    27	[Client tid=4 id=2] I want to play 5 more games. Sending scissors...
    28	[Client tid=3 id=1] received reply from nameserver: RPSServer=2
    29	[Client tid=3 id=1] I want to play 3 games. Sending signup...
    30	[Client tid=5 id=3] received signup ack
    31	[Client tid=5 id=3] I want to play 3 more games. Sending paper...
```

Client 2 receives the signup ack, and sends scissors. Client 1 (the one with the lowest priority), only just receives the response from the NameServer. It sends a signup request, but won't hear back for a while. Client 3, which was matched with client 2, also receives the signup ack and sends paper.

```
    32	[Client tid=4 id=2] I won!
    33	[Client tid=4 id=2] I want to play 4 more games. Sending paper...
    34	[Client tid=5 id=3] I lost :(
    35	[Client tid=5 id=3] I want to play 2 more games. Sending rock...
    36	~~~~~~~~~ press any key to continue ~~~~~~~~~
```

Our first game! Client 2 send scissors and client 3 sent paper, so client 2 wins. Both clients are informed of the result and send their next move. Since clients send their next move as soon as they see the results, client 2 sends their next move before client 3 has heard that it lost. The program pauses waiting for the user to press a key.

```
    37	[Client tid=4 id=2] I won!
    38	[Client tid=4 id=2] I want to play 3 more games. Sending rock...
    39	[Client tid=5 id=3] I lost :(
    40	[Client tid=5 id=3] I want to play 1 more game. Sending rock...
    41	~~~~~~~~~ press any key to continue ~~~~~~~~~
```

Client 2 sent paper and client 3 sent rock, so client 2 wins again. For the third game, both clients send rock. For client 3, it is their last game.

```
    42	[Client tid=4 id=2] it's a draw
    43	[Client tid=4 id=2] I want to play 2 more games. Sending rock...
    44	[Client tid=5 id=3] it's a draw
    45	[Client tid=5 id=3] sending quit
    46	~~~~~~~~~ press any key to continue ~~~~~~~~~
```

Since both clients sent rock in the last round, it's a draw. Client 2 wants to play another game, so it sends rock. But client 3 doesn't want to play any more games, so it sends quit.

```
    47	[RPSServer] tid 5 quit, but tid 3 is waiting. Matching tids 4 and 3
    48	[Client tid=3 id=1] received signup ack
    49	[Client tid=3 id=1] I want to play 3 more games. Sending scissors...
    50	[Client tid=5 id=3] exiting
    51	[Client tid=4 id=2] I won!
    52	[Client tid=4 id=2] I want to play 1 more game. Sending scissors...
    53	[Client tid=3 id=1] I lost :(
    54	[Client tid=3 id=1] I want to play 2 more games. Sending rock...
    55	~~~~~~~~~ press any key to continue ~~~~~~~~~
```

The RPS receives client 3's quit message and removes it from the game. Since client 1 (Tid 3) is still waiting to join a game, and client 2 is still playing, the RPSServer matches client 1 and 2, and finally sends client 1 the signup ack. Client 1 sends scissors, and since client 2 sent rock (in the last segment), client 2 wins. Clients 1 and 2 send another move, and client 3, after quitting, exits.

```
    56	[Client tid=4 id=2] I lost :(
    57	[Client tid=4 id=2] sending quit
    58	[Client tid=3 id=1] I won!
    59	[Client tid=3 id=1] I want to play 1 more game. Sending paper...
    60	~~~~~~~~~ press any key to continue ~~~~~~~~~
```

Client 1's rock beats client 2's scissors. Client 2 doesn't want to play anymore, so it sends quit. Client 1 doesn't know this and sends another move.

```
    61	[RPSServer] tid 4 quit, but no players are waiting.
    62	[Client tid=4 id=2] exiting
    63	[Client tid=3 id=1] other player quit! I guess I'll go home :(
    64	[Client tid=3 id=1] exiting
    65	Goodbye from choochoos kernel!
```

After client 2 quit, there are no other players waiting to join a game, so the RPSServer sends `OTHER_PLAYER_QUIT` to client 1. When client 1 receives this message, it exits. Client 2 also exits, after sending the quit message.
