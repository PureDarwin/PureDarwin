? [= Sender authd] claim only
* file /var/log/authd.log mode=0640 compress format=bsd rotate=seq file_max=5M all_max=20M
? [<= Level error] file /var/log/system.log
? [<= Level error] store
