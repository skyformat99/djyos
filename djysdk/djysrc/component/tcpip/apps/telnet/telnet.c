
//----------------------------------------------------
// Copyright (c) 2014, SHENZHEN PENGRUI SOFT CO LTD. All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. As a constituent part of djyos,do not transplant it to other software
//    without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------
// Copyright (c) 2014 ����Ȩ�����������������޹�˾���С�����Ȩ�˱���һ��Ȩ����
//
// �����Ȩ�����ʹ���߷��������������������£�����ʹ����ʹ�ü���ɢ����
// ������װԭʼ�뼰����λ��ִ����ʽ��Ȩ�������۴˰�װ�Ƿ񾭸�����Ȼ��
//
// 1. ���ڱ�����Դ�������ɢ�������뱣�������İ�Ȩ���桢�������б�����
//    ������������������
// 2. ���ڱ��׼�����λ��ִ����ʽ����ɢ���������������ļ��Լ�������������
//    ��ɢ����װ�е�ý�鷽ʽ����������֮��Ȩ���桢�������б����Լ�����
//    ������������
// 3. ��������Ϊ�����߲���ϵͳ����ɲ��֣�δ����ǰȡ���������ɣ���������ֲ����
//    �����߲���ϵͳ���������С�

// �����������������Ǳ�������Ȩ�������Լ�����������״��"as is"���ṩ��
// ��������װ�����κ���ʾ��Ĭʾ֮�������Σ������������ھ��������Լ��ض�Ŀ
// �ĵ�������ΪĬʾ�Ե�������Ȩ�����˼�������֮�����ߣ������κ�������
// ���۳�����κ��������塢���۴�����Ϊ���Լ��ϵ���޹�ʧ������������Υ
// Լ֮��Ȩ��������ʧ������ԭ��ȣ����𣬶����κ���ʹ�ñ�������װ��������
// �κ�ֱ���ԡ�����ԡ�ż���ԡ������ԡ��ͷ��Ի��κν�����𺦣�����������
// �������Ʒ������֮���á�ʹ����ʧ��������ʧ��������ʧ��ҵ���жϵȵȣ���
// �����κ����Σ����ڸ���ʹ���ѻ���ǰ��֪���ܻ���ɴ����𺦵���������Ȼ��
//-----------------------------------------------------------------------------
// =============================================================================
// Copyright (C) 2012-2020 ��԰�̱��Զ������޹�˾ All Rights Reserved
// ģ������: telnet.c
// ģ��汾: V1.00
// ������Ա: ZQF
// ����ʱ��: ����2:12:24/2015-1-26
// =============================================================================
// �����޸ļ�¼(���µķ�����ǰ��):
// <�汾��> <�޸�����>, <�޸���Ա>: <�޸Ĺ��ܸ���>
// =============================================================================
#include <sys/socket.h>

#include "../../tcpipconfig.h"

#define CN_TELNET_SERVERPORT  23
//which send a char string to the client,which specified by the setclientfocus
#include <driver.h>
#define CN_TELNET_DEVNAME  "telnet"
#define CN_TELNET_PATH     "/dev/telnet"
static u16  gTelnetClientEvttID = CN_EVTT_ID_INVALID;
#define CN_TELNET_CLIENT_MAX   0x10

#define CN_TELNET_USER_LEN  8

typedef struct __TelnetClient
{
        struct __TelnetClient *pre;
        struct __TelnetClient *nxt;
        int   sock;
        char  user[CN_TELNET_USER_LEN];
        char  passwd[CN_TELNET_USER_LEN];
}tagTelnetClient;

tagTelnetClient   *pTelnetClientQ = NULL;
struct  MutexLCB  *pTelnetClientSync = NULL;

static void __TelnetClientAdd(tagTelnetClient *client)
{
    if(Lock_MutexPend(pTelnetClientSync,CN_TIMEOUT_FOREVER))
    {
        if(NULL == pTelnetClientQ)
        {
            pTelnetClientQ = client;
        }
        else
        {
            pTelnetClientQ->pre = client;
            client->nxt = pTelnetClientQ;
            pTelnetClientQ= client;
        }
        Lock_MutexPost(pTelnetClientSync);
    }

    return;
}

static void __TelnetClientRemove(tagTelnetClient *client)
{
    if(Lock_MutexPend(pTelnetClientSync,CN_TIMEOUT_FOREVER))
    {
        if(client == pTelnetClientQ)
        {
            pTelnetClientQ = client->nxt;
            if(NULL != pTelnetClientQ)
            {
                pTelnetClientQ->pre = NULL;
            }
        }
        else
        {
            client->pre->nxt = client->nxt;
            if(NULL != client->nxt)
            {
                client->nxt->pre = client->pre;
            }
        }
        Lock_MutexPost(pTelnetClientSync);
    }

    return;
}


static bool_t    gTelnetConsole = false;
//install the device as an stdout device
static u32 __telnetWrite(ptu32_t tag,u8 *buf, u32 len,u32 offset, bool_t block,u32 timeout)
{
    tagTelnetClient *client;
    if(Lock_MutexPend(pTelnetClientSync,CN_TIMEOUT_FOREVER))
    {
        client = pTelnetClientQ;
        while(NULL != client)
        {
            sendexact(client->sock,buf,len);
            client = client->nxt;
        }
        Lock_MutexPost(pTelnetClientSync);
    }

    return len;
}

static u32 __telnetRead(ptu32_t tag,u8 *buf,u32 len,u32 offset,u32 timeout)
{
    return 0;
}

static bool_t __telnetConsoleInstall()
{

    if(NULL !=Driver_DeviceCreate(NULL,CN_TELNET_DEVNAME,NULL,NULL,__telnetWrite,__telnetRead,NULL,NULL,NULL))
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool_t __telnetConsoleSet(char *param)
{
   // OpenStdin(CN_TELNET_PATH);    //stdin device
    OpenStdout(CN_TELNET_PATH);     //stdout device
   // OpenStderr(CN_TELNET_PATH);   //stderr device

    return true;
}

#define CN_CLIENT_PREFIX   "DJYOS@:"
#define CN_CLIENT_WELCOM   "Welcome TELNET FOR DJYOS\n\rENTER QUIT TO EXIT\n\r"
#define CN_CLIENT_BUFLEN   64
extern bool_t Sh_ExecCommand(char *buf);
static ptu32_t __TelnetClientMain(void)
{
    int sock;
    int chars;  //how many chars in the buf
    char ch;
    int  rcvlen;
    u8   buf[CN_CLIENT_BUFLEN];

    tagTelnetClient  *client;


    Djy_GetEventPara((ptu32_t *)&client,NULL);
    sock = client->sock;
    sendexact(sock,CN_CLIENT_WELCOM,strlen(CN_CLIENT_WELCOM));
    sendexact(sock,CN_CLIENT_PREFIX,strlen(CN_CLIENT_PREFIX));
    chars = 0;
    while(1)
    {
        rcvlen = recv(sock,&ch,sizeof(ch),0);
        if(rcvlen == sizeof(ch))
        {
            //now check each char to do something
            if ((ch == '\r') || (ch == '\n'))
            {
                buf[chars] = '\0';
                if(chars > 1)
                {
                    if(0 == strcmp((const char *)buf,"quit"))
                    {
                        break;
                    }
                    else
                    {
                        //exe the command
                        Sh_ExecCommand((char *)buf);
                        sendexact(sock,CN_CLIENT_PREFIX,strlen(CN_CLIENT_PREFIX));
                        //reset the buffer
                        chars =0;
                    }
                }
            }
            else  //put the char in the buffer or move from the buf
            {
                if (ch == 8)    // Backspace
                {
                    if(chars > 0)
                    {
                        chars --;
                    }
                }
                else
                {
                    if(chars < CN_CLIENT_BUFLEN-1) //the last one will be '\0'
                    {
                        buf[chars] = ch;
                        chars++;
                    }
                    else
                    {
                        //exceed the buffer part ,will be ignored
                    }
                }
            }
        }
        else if(rcvlen == 0)
        {
            break;//bye bye
        }
        else
        {
            //do nothing ,no data yet
        }
    }

    __TelnetClientRemove(client);
    free(client);
    closesocket(sock);
    return 0;
}

static bool_t __CreateClientThread(ptu32_t para)
{
    bool_t result = false;
    u16 taskID;

    taskID = Djy_EventPop(gTelnetClientEvttID, NULL, 0, para, 0, 0);
    if(taskID != CN_EVENT_ID_INVALID)
    {
        result = true;
    }
    return result;
}
// =============================================================================
// �������ܣ���������������
// ���������
// ���������
// ����ֵ  ��
// ˵��    :
// =============================================================================
static ptu32_t __TelnetAcceptMain(void)
{
    struct sockaddr_in sa_server;
    int sockserver;
    int sockclient;
    int sockopt = 1;

    tagTelnetClient  *client;

    sockserver = socket(AF_INET, SOCK_STREAM, 0);
    sa_server.sin_family = AF_INET;
    sa_server.sin_port = htons(CN_TELNET_SERVERPORT);
    sa_server.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockserver, (struct sockaddr *) &sa_server, sizeof(sa_server)) < 0)
    {
        closesocket(sockserver);
        return 0;
    }

    if (listen(sockserver, CN_TELNET_CLIENT_MAX) != 0)
    {
        closesocket(sockserver);
        return 0;
    }
    //here we accept all the client
    while(1)
    {
        sockclient = accept(sockserver, NULL, 0);

        if(sockclient < 0)
        {
            goto ACCEPT_ERR;
        }
        sockopt= 1;
        if(0 != setsockopt(sockclient,IPPROTO_TCP,TCP_NODELAY,&sockopt,sizeof(sockopt)))
        {
        	goto   KEEPALIVE_ERR;
        }
        sockopt =1;
        if(0 != setsockopt(sockclient,SOL_SOCKET,SO_KEEPALIVE,&sockopt,sizeof(sockopt)))
        {
        	goto   KEEPALIVE_ERR;
        }
        client = (tagTelnetClient *)malloc(sizeof(tagTelnetClient));
        if(NULL == client)
        {
            goto CLIENT_MEM;
        }

        memset((void *)client,0,sizeof(tagTelnetClient));
        client->sock = sockclient;
        __TelnetClientAdd(client);

        if(false == __CreateClientThread((ptu32_t)client))
        {
            goto TASK_ERR;
        }
        if(false ==gTelnetConsole)
        {
            gTelnetConsole= __telnetConsoleSet(NULL);
        }
        goto ACCEPT_AGAIN;

TASK_ERR:
        __TelnetClientRemove(client);
        free((void *)client);
CLIENT_MEM:
        printk("%s:client memory malloc failed\n\r",__FUNCTION__);
KEEPALIVE_ERR:
        closesocket(sockclient);
ACCEPT_ERR:
        printk("%s:accept err\n\r",__FUNCTION__);
ACCEPT_AGAIN:
        printk("%s:accept complete\n\r",__FUNCTION__);
    }
    //anyway, could never reach here
    closesocket(sockserver);
    return 0;//never reach here
}
static ptu32_t TelnetClientMain()
{
	while(1)
	{
		__TelnetClientMain();
		Djy_WaitEvttPop(Djy_MyEvttId(),NULL,CN_TIMEOUT_FOREVER);
	}
	return 0;
}
//THIS IS TELNET SERVER MODULE FUNCTION
bool_t ServiceTelnetInit(ptu32_t para)
{
    bool_t result;
    u16    evttID;
    u16    eventID;

    result = false;
    pTelnetClientSync = Lock_MutexCreate(NULL);
    if(NULL == pTelnetClientSync)
    {
        goto __TELNET_INIT_EXIT;
    }
    evttID = Djy_EvttRegist(EN_CORRELATIVE, gTelnetAcceptPrior, 0, 1,
            __TelnetAcceptMain,NULL, gTelnetAcceptStack, "TelnetAcceptMain");
    eventID = Djy_EventPop(evttID, NULL, 0, 0, 0, 0);

    if((evttID == CN_EVTT_ID_INVALID)||(eventID == CN_EVENT_ID_INVALID))
    {
        printk("%s:Create the accept main failed:evttID:0x%04x eventID:0x%04x\n\r",\
                __FUNCTION__,evttID,eventID);
        goto __TELNET_INIT_EXIT;
    }

    gTelnetClientEvttID = Djy_EvttRegist(EN_CORRELATIVE, gTelnetProcessPrior, 0, 1,
    		TelnetClientMain,NULL, gTelnetProcessStack, "TelnetClientMain");

    return __telnetConsoleInstall();

__TELNET_INIT_EXIT:
    return result;
}




