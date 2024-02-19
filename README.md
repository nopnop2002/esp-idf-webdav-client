# esp-idf-webdav-client
WebDav client example for ESP-IDF.   
ESP-IDF contains a lot of example code, but there is no example to WebDav client.   
This project uses the WevDav protocol to manipulate files on a WebDav server.   
No libraries other than ESP-IDF are required to use WebDav.   

# Software requiment
ESP-IDF V4.4/V5.x.   
ESP-IDF V5.0 is required when using ESP32-C2.   
ESP-IDF V5.1 is required when using ESP32-C6.   

# Installation
```
git clone https://github.com/nopnop2002/esp-idf-webdav-client
cd esp-idf-webdav-client
idf.py menuconfig
idf.py flash monitor
```


# Configuration
Set the following items using menuconfig.

![config-top](https://github.com/nopnop2002/esp-idf-webdav-client/assets/6020549/f94309ff-f0b7-4309-9d5a-4a8457ab89ea)
![config-app](https://github.com/nopnop2002/esp-idf-webdav-client/assets/6020549/7bc90b6c-ce59-4366-ae24-06d9129cfb6b)## Wifi Setting

## WiFi Setting
![config-wifi](https://github.com/nopnop2002/esp-idf-webdav-client/assets/6020549/6b682110-34d5-49b8-83e6-5fa03f89a505)

## HTTP Server Setting
HTTP server is specified by one of the following.
- IP address   
 ```192.168.10.20```   
- mDNS host name   
 ```http-server.local```   
- Fully Qualified Domain Name   
 ```httpbin.org```

![config-http-1](https://github.com/nopnop2002/esp-idf-webdav-client/assets/6020549/6144964d-f0da-496f-9287-abe79bab4f0c)

You can use BASIC authentication.
![config-http-2](https://github.com/nopnop2002/esp-idf-webdav-client/assets/6020549/72b8d4f8-f91b-49be-a634-4ca4009e0404)


# Start WebDav server on Linux   
```
# Install wsgidav
sudo apt install git python3-pip python3-setuptools
python3 -m pip install -U pip
python3 -m pip install wsgidav cheroot

# Create public folder
mkdir $HOME/web-test

# Start WebDav server
wsgidav --host=0.0.0.0 --port=8080 --root=$HOME/web-test --auth anonymous
```

# How to use
- Open a new terminal and navigate to your public directory.   
```
$ cd $HOME/web-test
$ LANG=C ls -lR
.:
total 0
```

- Create new folder
```
W (6604) HTTP: Creating new foder on Webdav Server. Press Enter when ready.   
```

```
$ LANG=C ls -lR
.:
total 4
drwxrwxr-x 2 nop nop 4096 Feb 19 23:26 new_folder

./new_folder:
total 0
```

- Create new file
```
W (129134) HTTP: Creating new file on Webdav Server. Press Enter when ready.
```

```
$ LANG=C ls -lR
.:
total 4
drwxrwxr-x 2 nop nop 4096 Feb 19 23:29 new_folder

./new_folder:
total 20
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:29 file.txt
```

- Get file from server
```
W (14114) HTTP: Geting file on Webdav Server. Press Enter when ready.
```


- Copy file
```
W (37584) HTTP: Copying file on Webdav Server. Press Enter when ready.
```

```
$ LANG=C ls -lR
.:
total 4
drwxrwxr-x 2 nop nop 4096 Feb 19 23:33 new_folder

./new_folder:
total 40
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file.txt
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file2.txt
```

- Move file
```
W (19844) HTTP: Moveing file on Webdav Server. Press Enter when ready.
```

```
$ LANG=C ls -lR
.:
total 4
drwxrwxr-x 2 nop nop 4096 Feb 19 23:33 new_folder

./new_folder:
total 40
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file2.txt
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file3.txt
```

- Copy folder
```
W (40084) HTTP: Copying folder on Webdav Server. Press Enter when ready.
```

```
$ LANG=C ls -lR
.:
total 8
drwxrwxr-x 2 nop nop 4096 Feb 19 23:34 copy_folder
drwxrwxr-x 2 nop nop 4096 Feb 19 23:33 new_folder

./copy_folder:
total 40
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file2.txt
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file3.txt

./new_folder:
total 40
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file2.txt
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file3.txt
```

- Delete file
```
W (84614) HTTP: Deleting file on Webdav Server. Press Enter when ready.
```

```
$ LANG=C ls -lR
.:
total 8
drwxrwxr-x 2 nop nop 4096 Feb 19 23:34 copy_folder
drwxrwxr-x 2 nop nop 4096 Feb 19 23:35 new_folder

./copy_folder:
total 40
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file2.txt
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file3.txt

./new_folder:
total 20
-rw-rw-r-- 1 nop nop 20479 Feb 19 23:33 file2.txt
```

- Delete folder
```
W (118894) HTTP: Deleting folder on Webdav Server. Press Enter when ready.
```

```
$ LANG=C ls -lR
.:
total 0
```


# Limitaion
LOCK/UNLOCK is not supported.   

# Using curl   
You can manipulate files using curl.   
In addition to PUT/GET/DELETE, you can use PROPFIND/MKCOL/COPY/MOVE.   

- Read Files/Folders on Webdav Server:   
 The body is XML and follows the following schema.   
 https://personium.io/docs/en/apiref/307_Get_Property   
```
curl -i -X PROPFIND '192.168.10.42:8080'
```

- Creat new foder on Webdav Server:
```
curl -i -X MKCOL '192.168.10.42:8080/new_folder'
```

- Upload File on Webdav Server:
```
curl -i -X PUT -T 'file.txt' '192.168.10.42:8080/new_folder/file.txt'
```

- Download file on Webdav Server:
```
curl -i -X GET '192.168.10.42:8080/new_folder/file.txt'
```

- Copy file on Webdav Server:
```
curl -i -X COPY '192.168.10.42:8080/new_folder/file.txt' -H "Destination: /new_folder/file2.txt" -H "Overwrite: T"
```

- Move file on Webdav Server:
```
curl -i -X MOVE '192.168.10.42:8080/new_folder/file.txt' -H "Destination: /new_folder/file3.txt" -H "Overwrite: T"
```

- Copy folder on Webdav Server:
```
curl -i -X COPY '192.168.10.42:8080/new_folder' -H "Destination: /copy-folder"
```

- Delete file on Webdav Server:
```
curl -i -X DELETE '192.168.10.42:8080/copy-folder/file3.txt'
```

- Delete folder on Webdav Server:
```
curl -i -X DELETE '192.168.10.42:8080/copy-folder'
```

