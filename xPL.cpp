/*
 * xPL.Arduino v0.1, xPL Implementation for Arduino
 *
 * This code is parsing a xPL message stored in 'received' buffer
 * - isolate and store in 'line' buffer each part of the message -> detection of EOL character (DEC 10)
 * - analyse 'line', function of its number and store information in xpl_header memory
 * - check for each step if the message respect xPL protocol
 * - parse each command line
 *
 * Copyright (C) 2012 johan@pirlouit.ch, olivier.lebrun@gmail.com
 * Original version by Gromain59@gmail.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
 
#include "xPL.h"

#define XPL_LINE_MESSAGE_BUFFER_MAX 128	// max lenght of a line			// maximum command in a xpl message
#define XPL_END_OF_LINE 10

// define the line number identifier
#define XPL_MESSAGE_TYPE_IDENTIFIER	        1
#define XPL_OPEN_HEADER				2
#define XPL_HOP_COUNT				3
#define XPL_SOURCE				4
#define XPL_TARGET				5
#define XPL_CLOSE_HEADER			6
#define XPL_SCHEMA_IDENTIFIER		        7
#define XPL_OPEN_SCHEMA				8

// Heartbeat request class definition
//prog_char XPL_HBEAT_REQUEST_CLASS_ID[] PROGMEM = "hbeat";
//prog_char XPL_HBEAT_REQUEST_TYPE_ID[] PROGMEM = "request";
//prog_char XPL_HBEAT_ANSWER_CLASS_ID[] PROGMEM = "hbeat";
//prog_char XPL_HBEAT_ANSWER_TYPE_ID[] PROGMEM = "basic";  //app, basic
#define XPL_HBEAT_REQUEST_CLASS_ID  "hbeat"
#define XPL_HBEAT_REQUEST_TYPE_ID  "request"
#define XPL_HBEAT_ANSWER_CLASS_ID  "hbeat"
#define XPL_HBEAT_ANSWER_TYPE_ID  "basic"

/* xPL Class */
xPL::xPL()
{
  udp_port = XPL_UDP_PORT;
  
  SendExternal = NULL;

#ifdef ENABLE_PARSING
  AfterParseAction = NULL;

  last_heartbeat = 0;
  hbeat_interval = XPL_DEFAULT_HEARTBEAT_INTERVAL;
  xpl_accepted = XPL_ACCEPT_ALL;
#endif
}

xPL::~xPL()
{
}

void xPL::Begin(char* vendorId, char* deviceId, char* instanceId)
{
	strcpy(source.vendor_id, vendorId);
	strcpy(source.device_id, deviceId);
	strcpy(source.instance_id, instanceId);
}

void xPL::SendMessage(char *buffer)
{
  (*SendExternal)(buffer);
}

void xPL::SendMessage(xPL_Message *message)
{
    SendMessage(message->toString());
}

#ifdef ENABLE_PARSING
void xPL::Process()
{
	static bool bFirstRun = true;

	// check heartbeat + send
	if ((millis()-last_heartbeat >= (unsigned long)hbeat_interval * 60000)
		  || (bFirstRun && millis() > 3000))
	{
		SendHBeat();
		bFirstRun = false;
	}
}

void xPL::ParseInputMessage(char* buffer)
{
	// check xpl_accepted
	xPL_Message* xPLMessage = new xPL_Message();
	Parse(xPLMessage, buffer);

	// check if the message is an hbeat.request
	if (CheckHBeatRequest(xPLMessage))
	{
		SendHBeat();
	}

	if(AfterParseAction != NULL)
	{
	  (*AfterParseAction)(xPLMessage);  // call user function to execute action under the class
	}

	delete xPLMessage;
}

bool xPL::TargetIsMe(xPL_Message * message)
{
  if (strcmp(message->target.vendor_id, source.vendor_id) != 0)
    return false;

  if (strcmp(message->target.device_id, source.device_id) != 0)
    return false;

  if (strcmp(message->target.instance_id, source.instance_id) != 0)
    return false;

  return true;
}

void xPL::SendHBeat()
{
  last_heartbeat = millis();
  char buffer[XPL_MESSAGE_BUFFER_MAX];

  sprintf(buffer, "xpl-stat\n{\nhop=1\nsource=%s-%s.%s\ntarget=*\n}\n%s.%s\n{\ninterval=%d\n}\n", source.vendor_id, source.device_id, source.instance_id, XPL_HBEAT_ANSWER_CLASS_ID, XPL_HBEAT_ANSWER_TYPE_ID, hbeat_interval);

  (*SendExternal)(buffer);
}

bool xPL::CheckHBeatRequest(xPL_Message* xPLMessage)
{
  if (!TargetIsMe(xPLMessage))
    return false;

  if (strcmp_P(xPLMessage->schema.class_id, XPL_HBEAT_REQUEST_CLASS_ID) != 0)
    return false;

  if (strcmp_P(xPLMessage->schema.type_id, XPL_HBEAT_REQUEST_TYPE_ID) != 0)
    return false;

  return true;
}

void xPL::Parse(xPL_Message* xPLMessage, char* message)
{
    int len = strlen(message);

    byte j=0;
    byte line=0;
    int result=0;
    char buffer[XPL_LINE_MESSAGE_BUFFER_MAX] = "\0";

    // read each character of the message
    for(byte i=0; i<len; i++)
    {
        // load byte by byte in 'line' buffer, until '\n' is detected
        if(message[i]==XPL_END_OF_LINE) // is it a linefeed (ASCII: 10 decimal)
        {
            ++line;
            buffer[j]='\0';	// add the end of string id

            if(line<=XPL_OPEN_SCHEMA)
            {
                // first part: header and schema determination
                result=AnalyseHeaderLine(xPLMessage, buffer ,line); // we analyse the line, function of the line number inthe xpl message
            };

            if(line>XPL_OPEN_SCHEMA)
            {
                // second part: command line
                result=AnalyseCommandLine(xPLMessage, buffer, line-9); // we analyse the specific command line, function of the line number in the xpl message

                if(result==xPLMessage->command_count+1)
                {
                    break;
                };

            };

            if (result < 0)
            {
                break;
            };

            j=0; // reset the buffer pointer
            clearStr(buffer); // clear the buffer
        }
        else
        {
            // next character
            buffer[j++]=message[i];
        }
    }
}

byte xPL::AnalyseHeaderLine(xPL_Message* xPLMessage, char* buffer, byte line)
{
    switch (line)
    {

    case XPL_MESSAGE_TYPE_IDENTIFIER: //message type identifier

		if (memcmp(buffer,"xpl-",4)==0) //xpl
		{
			if (memcmp(buffer+4,"cmnd",4)==0) //type commande
			{
				xPLMessage->type=XPL_CMND;  //xpl-cmnd
			}
			else if (memcmp(buffer+4,"stat",4)==0) //type statut
			{
				xPLMessage->type=XPL_STAT;  //xpl-stat
			}
			else if (memcmp(buffer+4,"trig",4)==0) //type trigger
			{
				xPLMessage->type=XPL_TRIG;  //xpl-trig
			}
		}
        else
        {
            return 0;  //unknown message
        }
        return 1;
        break;

    case XPL_OPEN_HEADER: //header begin

        if (memcmp(buffer,"{",1)==0)
        {
            return 2;
        }
        else
                {
                    return -2;
                }
        break;

    case XPL_HOP_COUNT: //hop
        if (sscanf(buffer, "hop=%d", &xPLMessage->hop))
        {
            return 3;
        }
        else
                {
                    return -3;
                }
        break;

    case XPL_SOURCE: //source
        if (sscanf(buffer, "source=%[^-]-%[^'.'].%s", &xPLMessage->source.vendor_id, &xPLMessage->source.device_id, &xPLMessage->source.instance_id) == 3)
        {
          return 4;
        }
        else
                {
                  return -4;
                }
        break;

    case XPL_TARGET: //target

        if (sscanf(buffer, "target=%[^-]-%[^'.'].%s", &xPLMessage->target.vendor_id, &xPLMessage->target.device_id, &xPLMessage->target.instance_id) == 3)
        {
          return 5;
        }
        else
        {
          if(memcmp(xPLMessage->target.vendor_id,"*", 1) == 0)  // check if broadcast message
          {
            return 5;
          }
          else
          {
        	  return -5;
          }
        }
        break;

    case XPL_CLOSE_HEADER: //header end
        if (memcmp(buffer,"}",1)==0)
        {
            return 6;
        }
        else
                {
                    // Serial.print(" -> Header end not found");
                    return -6;
                }
        break;

    case XPL_SCHEMA_IDENTIFIER: //schema

        // xpl_header.schema=parse_schema_id(buffer);
        sscanf(buffer, "%[^'.'].%s", &xPLMessage->schema.class_id, &xPLMessage->schema.type_id);
        return 7;

        break;

    case XPL_OPEN_SCHEMA: //header begin
        if (memcmp(buffer,"{",1)==0)
        {
            return 8;
        }
        else
                {
                    // Serial.print(" -> Header begin not found");
                    return -8;
                }
        break;
    }

    return -100;
}

byte xPL::AnalyseCommandLine(xPL_Message * xPLMessage,char *buffer, byte command_line)
{
    if (memcmp(buffer,"}",1)==0) // End of schema
    {
        return xPLMessage->command_count+1;
    }
    else	// parse the next command
    {
    	struct_command newcmd;
        sscanf(buffer, "%[^'=']=%s", &newcmd.name, &newcmd.value);

        xPLMessage->AddCommand(newcmd.name, newcmd.value);

        return command_line;
    }
}
#endif
