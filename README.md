mysql-replication-listener
=========================

A listener to fetch binlog from mysql server.

Reference:
https://code.launchpad.net/mysql-replication-listener
https://github.com/wangmh/mysql-replication-listener

Install
--------------------
mysql-replication-listener uses cmake.

$ cmake .  -DCMAKE_INSTALL_PREFIX=/path/to/install

$ make

$ sudo make install


Notice
--------------------
We need to make mysql server to dump binlog-stream to us, so we need **REPLICATION SLAVE** privilege.
And We need to get master status, so we need **REPLICATION CLIENT** privilege.

If you want to set_position to a specified binlog-file and the position, we need to ensure the binlog-file and it's position are legal, so we need to excute "SHOW BINARY LOGS" on the server. Unfortunately, 
   In MySQL 5.1.63 and earlier, the SUPER privilege was required to use this statement. Starting with MYSQL 5.1.64, a user with the REPLICATION CLIENT privilege may also execute this statement.

So If you use MySQL 5.1.63 and earlier, you need **SUPER** privilege too.1;10;0c

