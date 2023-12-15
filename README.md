# Proxy Project


### Dependecny 
- libconfig 1.7.3
```
wget https://hyperrealm.github.io/libconfig/dist/libconfig-1.7.3.tar.gz
tar -xvf libconfig-1.7.3
cd libconfig-1.7.3
autoreconf
./configure 
make 
make check 
make install
```

- sds 
```
Already Included sds.c and sds.h sdsalloc.h testhelp.h
```

- thpool 
```
Already Included thpool.c and thpool.h
```

- list
```
Already Included list.h
```

### Config File 
```
# Proxy Project Config File 

service_name = "Home Proxy";
max_route_table = 20;

bind_port = 10000;

application = 
{
    clouds = (
        {
            name = "AWS Proxy";
            host = "127.0.0.1";
            port = 10001;          
            list = (15000, 15001); 
        },
        {
            name = "GCP Proxy";
            host = "127.0.0.1";
            port = 10002;         
            list = (15000, 15001);       
        }
    );
    
    services = (
        {
            name = "TCP Chat Service";
            host = "127.0.0.1"; 
            port = 20000; 
            forward_port = 20001;
            proto = 1;       # TCP Protocol
        },
        {
            name = "UDP Chat Service";
            host = "127.0.0.1"; 
            port = 30000; 
            forward_port = 30001
            proto = 2;       # UDP Protocol
        }
    );
}
```