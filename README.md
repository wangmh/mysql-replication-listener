MySQL Replication Listener
=========================

A listener to fetch binlog from mysql server.

Reference:
 - Origin repo: https://code.launchpad.net/mysql-replication-listener
 - We forked from: https://github.com/wangmh/mysql-replication-listener

Install
--------------------
mysql-replication-listener uses cmake, so you need cmake first.

We use asio for socket communication, and it involes some **boost** libraries, so maybe you need install boost(1.47 and newer).

And then:

$ cmake .  -DCMAKE_INSTALL_PREFIX=/path/to/install

$ make

$ make install


Notice
--------------------
We need to make mysql server to dump binlog-stream to us, so we need **REPLICATION SLAVE** privilege.
And We need to get master status, so we need **REPLICATION CLIENT** privilege.

If you want to set_position to a specified binlog-file and the position, we need to ensure the binlog-file and it's position are legal, so we need to excute "SHOW BINARY LOGS" on the server. Unfortunately, 
   In MySQL 5.1.63 and earlier, the SUPER privilege was required to use this statement. Starting with MYSQL 5.1.64, a user with the **REPLICATION CLIENT** privilege may also execute this statement.

So If you use MySQL 5.1.63 and earlier, you need **SUPER** privilege too. 

Also, We auth privilege before binlog_dump, so we will access database **mysql** for accout checking... That means you have to give at least **READ** privilege for database mysql ...


