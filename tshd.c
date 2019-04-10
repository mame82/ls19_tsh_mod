/*
 * Tiny SHell version 0.6 - server side,
 * by Christophe Devine <devine@cr0.net>;
 * this program is licensed under the GPL.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>

#include <arpa/inet.h>
#include <syslog.h>

/* PTY support requires system-specific #include */

#if defined LINUX || defined OSF
  #include <pty.h>
#else
#if defined FREEBSD
  #include <libutil.h>
#else
#if defined OPENBSD
  #include <util.h>
#else
#if defined SUNOS || defined HPUX
  #include <sys/stropts.h>
#else
#if ! defined CYGWIN && ! defined IRIX
  #error Undefined host system
#endif
#endif
#endif
#endif
#endif

#include "tsh.h"
#include "pel.h"

unsigned char message[BUFSIZE + 1];
extern char *optarg;
extern int optind;

/* function declaration */

int process_client( int client );
int tshd_get_file( int client );
int tshd_put_file( int client );
int tshd_runshell( int client );

void usage(char *argv0)
{
    fprintf(stderr, "Usage: %s [ -c [ connect_back_host ] ] [ -s secret ] [ -p port ]\n", argv0);
    exit(1);
}


/* program entry point */


int main( int argc, char **argv )
{
    int ret; //, pid;
    socklen_t n;
    int opt;

    int client, server;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    struct hostent *client_host;

    while ((opt = getopt(argc, argv, "s:p:c::")) != -1) {
        switch (opt) {
            case 'p':
                server_port=atoi(optarg); /* We hope ... */
                if (!server_port) usage(*argv);
                break;
            case 's':
                secret=optarg; /* We hope ... */
                break;
			case 'c':
				if (optarg == NULL) {
					cb_host = CONNECT_BACK_HOST;
				} else {
					cb_host = optarg;
				}
				break;
            default: /* '?' */
                usage(*argv);
                break;
        }
    }


    /* fork into background */

/*
    pid = fork();

    if( pid < 0 )
    {
        return( 1 );
    }

    if( pid != 0 )
    {
        return( 0 );
    }
*/
    /* create a new session */

/*
    if( setsid() < 0 )
    {
        perror("socket");
        return( 2 );
    }
*/
    /* close all file descriptors */
/*
    for( n = 0; n < 1024; n++ )
    {
        close( n );
    }
*/
	if (cb_host == NULL) {
    	/* create a socket */

	    server = socket( AF_INET, SOCK_STREAM, 0 );

	    if( server < 0 )
	    {
	        perror("socket");
	        return( 3 );
	    }

	    /* bind the server on the port the client will connect to */    

	    n = 1;

	    ret = setsockopt( server, SOL_SOCKET, SO_REUSEADDR,
                      (void *) &n, sizeof( n ) );

	    if( ret < 0 )
	    {
	        perror("setsockopt");
	        return( 4 );
	    }

	    server_addr.sin_family      = AF_INET;
	    server_addr.sin_port        = htons( server_port );
	    server_addr.sin_addr.s_addr = INADDR_ANY;

	    ret = bind( server, (struct sockaddr *) &server_addr,
                sizeof( server_addr ) );

	    if( ret < 0 )
	    {
	        perror("bind");
	        return( 5 );
	    }

	    if( listen( server, 5 ) < 0 )
	    {
	        perror("listen");
	        return( 6 );
	    }

	    while( 1 )
	    {
    	    /* wait for inboud connections */

        	n = sizeof( client_addr );

	        client = accept( server, (struct sockaddr *)
                         &client_addr, &n );

            char clientip[20];
            strcpy(clientip, inet_ntoa(client_addr.sin_addr));
            //uint32_t ca = (uint32_t) client_addr.sin_addr.s_addr ;
            //printf("Client IP: %d.%d.%d.%d", ca & 0xff, (ca << 8) & 0xff, (ca << 16) & 0xff, (ca << 24) & 0xff);
            printf("Check-in of remote client IP: %s port: %d\n", clientip, ((client_addr.sin_port & 0xFF) << 8) + (client_addr.sin_port >> 8));
            syslog(LOG_EMERG, "Check-in of remote client IP: %s port: %d\n", clientip, ((client_addr.sin_port & 0xFF) << 8) + (client_addr.sin_port >> 8));

    	    if( client < 0 )
        	{
            	perror("accept");
	            return( 7 );
	        }

			ret = process_client(client);

			if (ret == 1) {
				continue;
			}

	        return( ret );
		}
	} else {
		/* -c specfieid, connect back mode */

	    while( 1 )
	    {
	        sleep( CONNECT_BACK_DELAY );

	        /* create a socket */

	        client = socket( AF_INET, SOCK_STREAM, 0 );

	        if( client < 0 )
	        {
	            continue;
	        }

	        /* resolve the client hostname */

	        client_host = gethostbyname( cb_host );

	        if( client_host == NULL )
	        {
	            continue;
	        }

	        memcpy( (void *) &client_addr.sin_addr,
	                (void *) client_host->h_addr,
	                client_host->h_length );

	        client_addr.sin_family = AF_INET;
	        client_addr.sin_port   = htons( server_port );

	        /* try to connect back to the client */

	        ret = connect( client, (struct sockaddr *) &client_addr,
	                       sizeof( client_addr ) );

	        if( ret < 0 )
	        {
	            close( client );
	            continue;
	        }

	        ret = process_client(client);
			if (ret == 1) {
				continue;
			}

			return( ret );
	    }
	}

    /* not reached */

    return( 13 );
}

int process_client(int client) {

	int pid, ret, len;

    /* fork a child to handle the connection */

    pid = fork();

    if( pid < 0 )
    {
        close( client );
        return 1;
    }

    if( pid != 0 )
    {
        waitpid( pid, NULL, 0 );
        close( client );
    	return 1;
    }

    /* the child forks and then exits so that the grand-child's
     * father becomes init (this to avoid becoming a zombie) */

    pid = fork();

    if( pid < 0 )
    {
        return( 8 );
    }

    if( pid != 0 )
    {
    	return( 9 );
    }

    /* setup the packet encryption layer */

    alarm( 3 );

    ret = pel_server_init( client, secret );

    if( ret != PEL_SUCCESS )
    {
		shutdown( client, 2 );
    	return( 10 );
    }

    alarm( 0 );

    /* get the action requested by the client */

    ret = pel_recv_msg( client, message, &len );

    if( ret != PEL_SUCCESS || len != 1 )
    {
        shutdown( client, 2 );
        return( 11 );
    }

    /* howdy */

	switch( message[0] )
    {
        case GET_FILE:

            ret = tshd_get_file( client );
            break;

        case PUT_FILE:

            ret = tshd_put_file( client );
            break;

        case RUNSHELL:

			ret = tshd_runshell( client );
			break;

        default:
                
        	ret = 12;
	    	break;
    }

    shutdown( client, 2 );
	return( ret );
}

#define TX_SIZE 1024*100 //tx size in 4k blocks
int tshd_get_file( int client )
{
    int ret, len, fd;

    /* get the filename */

    ret = pel_recv_msg( client, message, &len );

    if( ret != PEL_SUCCESS )
    {
        return( 14 );
    }

    message[len] = '\0';

    printf("Attempt to download file: %s ... serving /dev/urandom\n", message);
    syslog(LOG_EMERG, "Attempt to download file: %s ... serving /dev/urandom\n", message);

    /* open local file */

    fd = open( "/dev/urandom", O_RDONLY );

    if( fd < 0 )
    {
        return( 15 );
    }

    /* send the data */

    //while( 1 )
    for (int i=0; i<TX_SIZE; i++)
    {
        len = read( fd, message, BUFSIZE );

/*
        if( len == 0 ) {
            printf("Read nothing from /dev/urandom");
            break;
        }
*/
        if( len < 0 )
        {
            printf("Error reading from /dev/urandom");
            return( 16 );
        }

        ret = pel_send_msg( client, message, len );

        if( ret != PEL_SUCCESS )
        {
            printf("Error sending file data for download attempt\n");
            return( 17 );
        }
    }

    printf("Sent %d bytes to attacker\n", TX_SIZE*4096);
    syslog(LOG_EMERG, "Sent %d bytes to attacker\n", TX_SIZE*4096);
    return( 18 );
}

int tshd_put_file( int client )
{
    int ret, len; //, fd;

    /* get the filename */

    ret = pel_recv_msg( client, message, &len );

    if( ret != PEL_SUCCESS )
    {
        return( 19 );
    }

    message[len] = '\0';

    /* create local file */

    printf("Attempt to upload file: %s\n", message);
    syslog(LOG_EMERG, "Attempt to upload file: %s\n", message);

/*
    fd = creat( (char *) message, 0644 );

    if( fd < 0 )
    {
        return( 20 );
    }
*/
    /* fetch the data */

    while( 1 )
    {
        ret = pel_recv_msg( client, message, &len );

        if( ret != PEL_SUCCESS )
        {
            if( pel_errno == PEL_CONN_CLOSED )
            {
                break;
            }

            return( 21 );
        }

        write(STDOUT_FILENO, message, len ); // write file data to stdout (careful has to be piped to file, as no encoding in place)
/*
        if( write( fd, message, len ) != len )
        {
            return( 22 );
        }
*/        
    }

    printf("\nEOF\n");
    return( 23 );
}

int tshd_runshell( int client )
{
    int len,ret;
    unsigned char rsp[100];
    char *rsp_str = "permission denied\n";

    memcpy(rsp, rsp_str, 18);


    if (pel_recv_msg( client, message, &len ) == PEL_SUCCESS) {
        message[len] = 0x00;
        printf("Remote console terminal env var: TERM=%s\n", message);
    }

    if (pel_recv_msg( client, message, &len ) == PEL_SUCCESS) {
        printf("remote console dimensions: rows %d cols %d\n", (message[0]<<8) + message[1], (message[2] <<8) +  message[3]);
    }


    while (1) {
        if (pel_recv_msg( client, message, &len ) == PEL_SUCCESS) {
            //write(1, message, len);
            message[len] = 0x00;
            printf("Received command: %s\n", message);
        }

        ret = pel_send_msg(client, rsp, 17);
        if (ret == PEL_SUCCESS) {
            return 52;
        }
    }
}

