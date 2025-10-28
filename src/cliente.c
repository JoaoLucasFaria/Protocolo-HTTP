#include "http.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

static void parse_url(const char* url, char host[256], char path[1024], char port[8]){
    // espera "http://host[:porto]/caminho"
    if(strncmp(url,"http://",7)!=0){ fprintf(stderr,"URL inválida\n"); exit(2); }
    const char *p = url+7;
    const char *slash = strchr(p,'/');
    const char *hostend = slash ? slash : url+strlen(url);
    const char *colon = memchr(p, ':', hostend-p);
    size_t hlen = (colon?colon:hostend)-p; if(hlen>=255){fprintf(stderr,"host grande\n"); exit(2);}
    strncpy(host,p,hlen); host[hlen]='\0';
    if(colon){ size_t plen = hostend-colon-1; if(plen>=7){fprintf(stderr,"porto grande\n"); exit(2);}
      strncpy(port, colon+1, plen); port[plen]='\0';
    }else strcpy(port,"80");
    if(slash) snprintf(path,1024,"%s",slash); else strcpy(path,"/");
}

int main(int argc,char**argv){
    if(argc!=2){ fprintf(stderr,"Uso: %s http://host[:porta]/caminho\n",argv[0]); return 2; }
    char host[256], path[1024], port[8]; parse_url(argv[1],host,path,port);

    struct addrinfo hints={0}, *res;
    hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM;
    if(getaddrinfo(host,port,&hints,&res)!=0){ perror("getaddrinfo"); return 2; }
    int fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if(fd<0){ perror("socket"); return 2; }
    if(connect(fd,res->ai_addr,res->ai_addrlen)<0){ perror("connect"); return 1; }
    freeaddrinfo(res);

    dprintf(fd,"GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: meu_navegador/1.0\r\nConnection: close\r\n\r\n",path,host);

    // abre arquivo destino baseado no caminho
    const char *name = strrchr(path,'/'); name = (name && *(name+1)) ? name+1 : "index.html";
    FILE *out=fopen(name,"wb"); if(!out){ perror("fopen"); close(fd); return 2; }

    // lê cabeçalho para checar status
    char buf[4096]; ssize_t n; int header_done=0, status=0;
    char header[65536]=""; size_t hlen=0;
    while((n=recv(fd,buf,sizeof(buf),0))>0){
        if(!header_done){
            memcpy(header+hlen,buf,n); hlen+=n; header[hlen]=0;
            char *pp = strstr(header,"\r\n\r\n");
            if(pp){
                header_done=1;
                // status
                char *sp = strchr(header,' '); status = sp?atoi(sp+1):0;
                size_t body_off = (pp+4-header);
                fwrite(header+body_off,1,hlen-body_off,out);
            }
        }else{
            fwrite(buf,1,n,out);
        }
    }
    fclose(out); close(fd);
    if(status!=200){ fprintf(stderr,"erro HTTP %d\n",status); return 1; }
    return 0;
}