/*
 * Copyright (C) 2014 - plutoo
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util.h"
#include "handles.h"
#include "mem.h"
#include "arm11.h"

#include "http/HTTPClient.h"

#include "service_macros.h"

#define HTTP_CLIENT_BUFFER_SIZE     8192

SERVICE_START(http_c);

#ifdef _WIN32
SERVICE_CMD(0x00010044)
{

#ifdef _WIN32
    // Windows Specific - Sockets initialization
    unsigned short      wVersionRequested;
    WSADATA             wsaData;
    UINT32              nErr = 0;
    // We want to use Winsock v 1.2 (can be higher)
    wVersionRequested = MAKEWORD(1, 2);
    nErr = WSAStartup(wVersionRequested, &wsaData);
#endif

    DEBUG("Initialize %08x\n", CMD(1));
    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x00020082)
{
    u32 size = CMD(1);
    DEBUG("CreateContext %08x %02x %08x\n", size, CMD(2), CMD(4));
    u32 handle = handle_New(HANDLE_TYPE_HTTPCont, 0);

    handleinfo* h = handle_Get(handle);
    if (h == NULL) {
        DEBUG("failed to get newly created Handle\n");
        PAUSE();
        return -1;
    }
    h->misc_ptr[0] = NULL;
    h->misc_ptr[1] = NULL;
    h->misc_ptr[2] = NULL;

    u8* b = malloc(size + 1);
    if (b == NULL) {
        ERROR("Not enough mem.\n");
        return -1;
    }
    memset(b, 0, size + 1);
    if (mem_Read(b, CMD(4), size) != 0) {
        ERROR("mem_Read failed.\n");
        free(b);
        return -1;
    }

    h->misc_ptr[0] = malloc(size + 1);
    strncpy(h->misc_ptr[0], b, size + 1);

    DEBUG("address %s\n", h->misc_ptr[0]);

    RESP(2, handle);
    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x80042)
{
    DEBUG("InitializeConnectionSession %08x --stubed--\n", CMD(1));
    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x00090040)
{
    DEBUG("BeginRequest %08x\n", CMD(1));

    handleinfo* h = handle_Get(CMD(1));
    if (h == NULL) {
        DEBUG("failed to get newly created Handle\n");
        PAUSE();
        return -1;
    }

    INT32                  nRetCode;
    HTTP_SESSION_HANDLE     pHTTP;
    pHTTP = HTTPClientOpenRequest(0);

    if ((nRetCode = HTTPClientSetVerb(pHTTP, VerbGet)) != HTTP_CLIENT_SUCCESS) {
        RESP(1, 0xFFFFFFFF); // Result
        return 0;
    }

    // Set authentication

    if ((nRetCode = HTTPClientSetAuth(pHTTP, AuthSchemaNone, NULL)) != HTTP_CLIENT_SUCCESS) {
        RESP(1, 0xFFFFFFFF); // Result
        return 0;
    }
    int i;
    for (i = 0; i < (int)h->misc[2]; i++)
    {
        if (HTTPClientAddRequestHeaders(pHTTP, *((char**)(h->misc_ptr[2]) + i * 2), *((char**)(h->misc_ptr[2]) + i * 2 + 1), true))
        {
            RESP(1, 0xFFFFFFFF); // Result
            return 0;
        }
    }
    // Send a request for the home page
    if(HTTPClientSendRequest(pHTTP, h->misc_ptr[0], NULL, 0, FALSE, 0, 0) != HTTP_CLIENT_SUCCESS) {
        RESP(1, 0xFFFFFFFF); // Result
        return 0;
    }

    h->misc[0] = HTTPClientRecvResponse(pHTTP, 3);

    nRetCode = h->misc[0];

    h->misc[1] = 0;

    h->misc_ptr[1] = NULL;

    h->misc[2] = 0;

    h->misc_ptr[2] = NULL;

    int nSize = HTTP_CLIENT_BUFFER_SIZE;

    while (nRetCode == HTTP_CLIENT_SUCCESS || nRetCode == HTTP_CLIENT_ERROR_BAD_STATE) {
        h->misc_ptr[1] = realloc(h->misc_ptr[1], h->misc[1] + HTTP_CLIENT_BUFFER_SIZE);
        // Set the size of our buffer
        int nSize = HTTP_CLIENT_BUFFER_SIZE;

        // Get the data
        nRetCode = HTTPClientReadData(pHTTP, (u8*)h->misc_ptr[1] + h->misc[1], nSize, 0, &nSize);
        h->misc[1] += nSize;
    }
    HTTPClientCloseRequest(&pHTTP);


    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x220040)
{
    DEBUG("GetResponseStatusCode %08x\n", CMD(1));

    handleinfo* h = handle_Get(CMD(1));
    if (h == NULL) {
        DEBUG("failed to get newly created Handle\n");
        PAUSE();
        return -1;
    }
    if (h->misc[0] == HTTP_CLIENT_SUCCESS) //todo
        RESP(2, 200);
    else
        RESP(2, 404);
    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x00060040)
{
    DEBUG("GetDownloadSizeState %08x\n", CMD(1));

    handleinfo* h = handle_Get(CMD(1));
    if (h == NULL) {
        DEBUG("failed to get newly created Handle\n");
        PAUSE();
        return -1;
    }
    RESP(3, h->misc[1]); //buffer is full
    RESP(2, h->misc[1]);
    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x000B0082)
{
    DEBUG("GetDownloadSizeState %08x %08x %08x\n", CMD(1), CMD(2), CMD(4));

    handleinfo* h = handle_Get(CMD(1));
    if (h == NULL) {
        DEBUG("failed to get newly created Handle\n");
        PAUSE();
        return -1;
    }

    if (mem_Write(h->misc_ptr[1], CMD(4), CMD(2)) != 0) {
        ERROR("mem_Write failed.\n");
        free(h->misc_ptr[1]);
        return -1;
    }

    RESP(1, 0); // Result
    return 0;
}

SERVICE_CMD(0x00030040)
{
    DEBUG("CloseContext %08x --stub--\n", CMD(1));

    handleinfo* h = handle_Get(CMD(1));
    if (h == NULL) {
        DEBUG("failed to get newly created Handle\n");
        PAUSE();
        return -1;
    }

    if(h->misc_ptr[0]);
    free(h->misc_ptr[0]);
    if (h->misc_ptr[1]);
    free(h->misc_ptr[1]);

    RESP(1, 0); // Result
    return 0;
}

SERVICE_CMD(0x002D0000)
{
    DEBUG("unk2D --stub--\n");

    RESP(2, 0); // unk
    RESP(1, 0); // Result
    return 0;
}

SERVICE_CMD(0x00300080)
{
    DEBUG("unk30 %08X %08X --stub--\n", CMD(1), CMD(2));

    RESP(2, 0); // unk
    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x00330040)
{
    DEBUG("unk33 %08X --stub--\n", CMD(1));

    RESP(2, 0); // unk
    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x000E0040) //SetProxyDefault
{
    DEBUG("SetProxyDefault %08X --stub--\n", CMD(1));

    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x001100C4) //AddRequestHeader
{
    DEBUG("AddRequestHeader %08X %08X %08X %08X %08X\n", CMD(1), CMD(2), CMD(3),CMD(5),CMD(7));

    //HTTPClientAddRequestHeaders

    handleinfo* h = handle_Get(CMD(1));
    if (h == NULL) {
        DEBUG("failed to get newly created Handle\n");
        PAUSE();
        return -1;
    }

    u32 size1 = CMD(2);
    u32 size2 = CMD(3);

    u8* a = malloc(size1 + 1);
    if (a == NULL) {
        ERROR("Not enough mem.\n");
        return -1;
    }
    memset(a, 0, size1 + 1);
    if (mem_Read(a, CMD(5), size1) != 0) {
        ERROR("mem_Read failed.\n");
        free(a);
        return -1;
    }

    u8* b = malloc(size2 + 1);
    if (b == NULL) {
        ERROR("Not enough mem.\n");
         return -1;
    }
    memset(b, 0, size2 + 1);
    if (mem_Read(b, CMD(7), size2) != 0) {
        ERROR("mem_Read failed.\n");
        free(b);
        return -1;
    }


    h->misc_ptr[2] = (char*)realloc(h->misc_ptr[2], sizeof(char*) * 2 * (h->misc[2] + 1));
    *((char**)(h->misc_ptr[2]) + h->misc[2] * 2) = a;
    *((char**)(h->misc_ptr[2]) + h->misc[2] * 2 + 1) = b;
    h->misc[2]++;

    DEBUG("name %s val %s\n", a,b);

    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x00260080)
{
    DEBUG("unk26 --stub-- %08X %08X\n", CMD(1), CMD(2));

    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x002a0040)
{
    DEBUG("unk 2a --stub-- %08X %08X\n", CMD(1));

    RESP(2, 0); // unk u32
    RESP(1, 0); // Result
    return 0;
}

#endif
SERVICE_END();
