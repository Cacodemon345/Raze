//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//-------------------------------------------------------------------------

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include "compat.h"
#include <windows.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <shellapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "renderlayer.h"

int32_t G_GetVersionFromWebsite(char *buffer)
{
    int32_t wsainitialized = 0;
    int32_t bytes_sent, i=0, j=0;
    struct sockaddr_in dest_addr;
    struct hostent *h;
    char *host = "eduke32.sourceforge.net";
    char *req = "GET http://eduke32.sourceforge.net/VERSION HTTP/1.0\r\n\r\n\r\n";
    char tempbuf[2048],otherbuf[16],ver[16];
    SOCKET mysock;

#ifdef _WIN32
    if (wsainitialized == 0)
    {
        WSADATA ws;

        if (WSAStartup(0x101,&ws) == SOCKET_ERROR)
        {
//            initprintf("update: Winsock error in G_GetVersionFromWebsite() (%d)\n",errno);
            return(0);
        }
        wsainitialized = 1;
    }
#endif

    if ((h=gethostbyname(host)) == NULL)
    {
//        initprintf("update: gethostbyname() error in G_GetVersionFromWebsite() (%d)\n",h_errno);
        return(0);
    }

    dest_addr.sin_addr.s_addr = ((struct in_addr *)(h->h_addr))->s_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(80);

    memset(&(dest_addr.sin_zero), '\0', 8);


    mysock = socket(PF_INET, SOCK_STREAM, 0);

    if (mysock == INVALID_SOCKET)
    {
//        initprintf("update: socket() error in G_GetVersionFromWebsite() (%d)\n",errno);
        return(0);
    }
    initprintf("Connecting to http://%s\n",host);
    if (connect(mysock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr)) == SOCKET_ERROR)
    {
        //      initprintf("update: connect() error in G_GetVersionFromWebsite() (%d)\n",errno);
        return(0);
    }

    bytes_sent = send(mysock, req, strlen(req), 0);
    if (bytes_sent == SOCKET_ERROR)
    {
        //    initprintf("update: send() error in G_GetVersionFromWebsite() (%d)\n",errno);
        return(0);
    }

    //    initprintf("sent %d bytes\n",bytes_sent);
    recv(mysock, (char *)&tempbuf, sizeof(tempbuf), 0);
    closesocket(mysock);

    Bmemcpy(&otherbuf,&tempbuf,sizeof(otherbuf));

    strtok(otherbuf," ");
    if (Batol(strtok(NULL," ")) == 200)
    {
        for (i=0; (unsigned)i<strlen(tempbuf); i++) // HACK: all of this needs to die a fiery death; we just skip to the content
        {
            // instead of actually parsing any of the http headers
            if (i > 4)
                if (tempbuf[i-1] == '\n' && tempbuf[i-2] == '\r' && tempbuf[i-3] == '\n' && tempbuf[i-4] == '\r')
                {
                    while (j < 9)
                    {
                        ver[j] = tempbuf[i];
                        i++, j++;
                    }
                    ver[j] = '\0';
                    break;
                }
        }

        if (j)
        {
            strcpy(buffer,ver);
            return(1);
        }
    }
    return(0);
}
#endif
