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

xPL_Message::xPL_Message()
{
	command = NULL;
	command_count = 0;
    //ClearData();
}

xPL_Message::~xPL_Message()
{
	 if(command != NULL)
	    	free(command);
}

/*void xPL_Message::ClearData()
{
    type = -1;
    hop = -1;

    clearStr(source.vendor_id);
    clearStr(source.device_id);
    clearStr(source.instance_id);
    clearStr(target.vendor_id);
    clearStr(target.device_id);
    clearStr(target.instance_id);
    clearStr(schema.class_id);
    clearStr(schema.type_id);

    if(command != NULL)
    	free(command);

    command = NULL;
}*/

void xPL_Message::AddCommand(char* _name, char* _value)
{
	command = (struct_command*)realloc ( command, (++command_count) * sizeof(struct_command) );

	struct_command newcmd;
	strcpy(newcmd.name, _name);
	strcpy(newcmd.value, _value);

	command[command_count-1] = newcmd;
}

void xPL_Message::AddCommand(char* _name, int _value)
{
	char buffer[50];
	sprintf(buffer,"%d",_value);
	AddCommand(_name, buffer);
}

void xPL_Message::Parse(char *message, unsigned short len)
{
    int j=0;
    int line=0;
    int result=0;
    char buffer[XPL_LINE_MESSAGE_BUFFER_MAX] = "\0";

    //ClearData();

    // read each character of the message
    for(int i=0; i<len; i++)
    {
        // load byte by byte in 'line' buffer, until '\n' is detected
        if(message[i]==XPL_END_OF_LINE) // is it a linefeed (ASCII: 10 decimal)
        {
            line++;
            buffer[j]='\0';	// add the end of string id

            if(line<=XPL_OPEN_SCHEMA)
            {
                // first part: header and schema determination
                result=AnalyseHeaderLine(buffer, line); // we analyse the line, function of the line number inthe xpl message
            };

            if(line>XPL_OPEN_SCHEMA)
            {
                // second part: command line
                result=AnalyseCommandLine(buffer, line-9); // we analyse the specific command line, function of the line number in the xpl message

                if(result==command_count+1)
                {
                    //Serial.print(millis());
                    //Serial.println(" - Data analyse finished");
                    break;
                };

            };

            if (result<0)
            {
                // if the analyse of the line return and error
                //ClearData();
                //Serial.print(millis());
                //Serial.print(" - xPL message traitment stopped at line ");
                //Serial.print(line);
                //Serial.print(" error code ");
                //Serial.print(result);
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

char *xPL_Message::toString()
{
  char message_buffer[XPL_MESSAGE_BUFFER_MAX];

  char message_type[9];
  clearStr(message_buffer);
  
  switch(type)
  {
    case (XPL_CMND):
      sprintf(message_type, "xpl-cmnd");
      break;
    case (XPL_STAT):
      sprintf(message_type, "xpl-stat");
      break;
    case (XPL_TRIG):
      sprintf(message_type, "xpl-trig");
      break;
  }  

  // correction &hop dans le sprintf
  if(memcmp(target.vendor_id,"*", 1) == 0)  // check if broadcast message
  {
    sprintf(message_buffer, "%s\n{\nhop=1\nsource=%s-%s.%s\ntarget=%s\n}\n%s.%s\n{\n", message_type, source.vendor_id, source.device_id, source.instance_id, "*", schema.class_id, schema.type_id);
  }
  else
  {
    sprintf(message_buffer, "%s\n{\nhop=1\nsource=%s-%s.%s\ntarget=%s-%s.%s\n}\n%s.%s\n{\n", message_type, source.vendor_id, source.device_id, source.instance_id, target.vendor_id, target.device_id, target.instance_id, schema.class_id, schema.type_id);    
  }
  
  for (int i=0; i<command_count; i++)
  {
    strcat(message_buffer, command[i].name);
    strcat(message_buffer, "=");
    strcat(message_buffer, command[i].value);
    strcat(message_buffer, "\n"); 
  }
  
  strcat(message_buffer, "}\n");
  
  return message_buffer;
}

bool xPL_Message::IsSchema(char *class_id, char *type_id)
{
  if (strcmp(schema.class_id, class_id) == 0)
  {
    if (strcmp(schema.type_id, type_id) == 0)
    {
      return true;
    }   
  }  

  return false;
}

int xPL_Message::AnalyseHeaderLine(char *buffer, int line)
{

    switch (line)
    {

    case XPL_MESSAGE_TYPE_IDENTIFIER: //message type identifier

        if (memcmp(buffer,"xpl-cmnd",8)==0) //type commande
        {
            type=XPL_CMND;  //xpl-cmnd
        }
        else if (memcmp(buffer,"xpl-stat",8)==0) //type statut
        {
            type=XPL_STAT;  //xpl-stat
        }
        else if (memcmp(buffer,"xpl-trig",8)==0) //type trigger
        {
            type=XPL_TRIG;  //xpl-trig
        }
        else
        {
            return -1;  //unknown message
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
        if (sscanf(buffer, "hop=%d", &hop))
        {
            return 3;
        }
        else
        {
            return -3;
        }
        break;

    case XPL_SOURCE: //source
        if (sscanf(buffer, "source=%[^-]-%[^'.'].%s", &source.vendor_id, &source.device_id, &source.instance_id) == 3)
        {
          return 4;
        }
        else
        {
          return -4;
        }
        break;

    case XPL_TARGET: //target

        if (sscanf(buffer, "target=%[^-]-%[^'.'].%s", &target.vendor_id, &target.device_id, &target.instance_id) == 3)        
        {
          return 5;  
        }
        else
        {
          if(memcmp(target.vendor_id,"*", 1) == 0)  // check if broadcast message
          {
            return 5;
          }  
          else
          {
            return -5; 
          }
        }

        /*if(!strcmp(this.target.vendor_id,"*"))
        {
            Serial.print(millis());
            Serial.println(" --> broadcast message");
            return 5;
        }
        else if(!strcmp(this.target.vendor_id, vendor_id) && !strcmp(this.target.instance_id, instance_id)) // check if it's the good vendor and instance id
        {
            Serial.print(millis());
            Serial.println(" --> this target message");
            return 5;
        }
        else
        {
            Serial.print(millis());
            Serial.println(" --> other target message");
            return -5;
        }*/

        return 5;
        break;

    case XPL_CLOSE_HEADER: //header end
        if (memcmp(buffer,"}",1)==0)
        {
            // Serial.print(" -> Header end");
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
        sscanf(buffer, "%[^'.'].%s", &schema.class_id, &schema.type_id);

        // Serial.println();
        // Serial.print("   |-> schema class id -> ");
        // Serial.println(xpl_header.schema.class_id);
        // Serial.print("   |-> schema type id -> ");
        // Serial.println(xpl_header.schema.type_id);
        return 7;

        break;

    case XPL_OPEN_SCHEMA: //header begin
        if (memcmp(buffer,"{",1)==0)
        {
            // Serial.print(" -> Header begin");
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

int xPL_Message::AnalyseCommandLine(char *buffer, int command_line)
{
    if (memcmp(buffer,"}",1)==0) // End of schema
    {
        return command_count+1;
    }
    else	// parse the next command
    {
    	struct_command newcmd;
        sscanf(buffer, "%[^'=']=%s", &newcmd.name, &newcmd.value);
        //command_count++;

        AddCommand(newcmd.name, newcmd.value);

        return command_line;
    }
}


/* xPL Class */
xPL::xPL()
{
  hbeat_interval = XPL_DEFAULT_HEARTBEAT_INTERVAL;
  udp_port = XPL_UDP_PORT;
  xpl_accepted = XPL_ACCEPT_ALL;
  
  //ClearData();
  
  last_heartbeat = 0;
}

void xPL::Begin(char *vendorid, char *deviceid, char *instanceid)
{
  strcpy(source.vendor_id, vendorid);
  strcpy(source.device_id, deviceid);
  strcpy(source.instance_id, instanceid);    
}

void xPL::Process()
{
	static bool bFirstRun = true;

  //clearStr(xPLMessageBuff);

  // check heartbeat + send
  if ((millis()-last_heartbeat >= (unsigned long)hbeat_interval * 60000)
		  || (bFirstRun && millis() > 3000))
  {
    SendHBeat();
    bFirstRun = false;
  }
}

/*void xPL::ClearData()
{
  clearStr(source.vendor_id);
  clearStr(source.device_id);
  clearStr(source.instance_id);
}*/

void xPL::ParseInputMessage(char* buffer)
{
  // check xpl_accepted
  xPL_Message *message = new xPL_Message();
  message->Parse(buffer, strlen(buffer));
    
  // check if the message is an hbeat.request
  if (CheckHBeatRequest(message) == 1)
  {    
    SendHBeat(); 
  }
  
  if(AfterParseAction != NULL)
	  (*AfterParseAction)(message);  // call user function to execute action under the class

  delete message;
}

void xPL::SendMessage(char *buffer)
{
  (*SendExternal)(buffer, strlen(buffer));
}

void xPL::SendMessage(xPL_Message *message)
{
    SendMessage(message->toString());
}

bool xPL::TargetIsMe(xPL_Message * message)
{
  if (strcmp(message->target.vendor_id, source.vendor_id) != 0)
    return false;
  
  if (strcmp(message->target.device_id, source.device_id) != 0)
    return false;
  
  if (strcmp(message->target.instance_id, source.instance_id) != 0)
    return false;
    
    // no check command=request
  
  return true;
}

void xPL::SetSourceVendorId(char *vendorid)
{
  strncpy(source.vendor_id, vendorid, XPL_VENDOR_ID_MAX);  
  source.vendor_id[XPL_VENDOR_ID_MAX+1] = '\0';
}

void xPL::SetSourceDeviceId(char *deviceid)
{
  strncpy(source.device_id, deviceid, XPL_DEVICE_ID_MAX);
  source.device_id[XPL_DEVICE_ID_MAX+1] = '\0';
}

void xPL::SetSourceInstanceId(char *instanceid)
{
  strncpy(source.instance_id, instanceid, XPL_INSTANCE_ID_MAX); 
  source.instance_id[XPL_INSTANCE_ID_MAX+1] = '\0';
}

void xPL::SetHBeatInterval(unsigned short interval)
{
  hbeat_interval = interval; 
}

void xPL::SetUdpPort(unsigned short udpport)
{
  udp_port = udpport; 
}

void xPL::SendHBeat()
{
  last_heartbeat = millis();
  char buffer[XPL_MESSAGE_BUFFER_MAX];
  
  //if (memcmp(xPLMessage.target.vendor_id, "*", 1) == 0)
  //{
    sprintf(buffer, "xpl-stat\n{\nhop=1\nsource=%s-%s.%s\ntarget=*\n}\n%s.%s\n{\ninterval=%d\n}\n", source.vendor_id, source.device_id, source.instance_id, XPL_HBEAT_ANSWER_CLASS_ID, XPL_HBEAT_ANSWER_TYPE_ID, hbeat_interval); 
  /*}
  else
  {
    sprintf(buffer, "xpl-stat\n{\nhop=1\nsource=%s-%s.%s\ntarget=%s-%s.%s\n}\n%s.%s\n{\ninterval=%d\n}\n", source.vendor_id, source.device_id, source.instance_id, xPLMessage.source.vendor_id, xPLMessage.source.device_id, xPLMessage.source.instance_id, XPL_HBEAT_ANSWER_CLASS_ID, XPL_HBEAT_ANSWER_TYPE_ID, hbeat_interval);
  }*/
   
  (*SendExternal)(buffer, strlen(buffer));  //call user function to send datas
}

bool xPL::CheckHBeatRequest(xPL_Message * message)
{  
  if (TargetIsMe(message) == 0)
    return false;

  if (strcmp(message->schema.class_id, XPL_HBEAT_REQUEST_CLASS_ID) != 0)
    return false;
    
  if (strcmp(message->schema.type_id, XPL_HBEAT_REQUEST_TYPE_ID) != 0)
    return false;
     
  return true;
}
