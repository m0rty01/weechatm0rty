/*
 * Copyright (c) 2003-2007 by FlashCode <flashcode@flashtux.org>
 * See README for License detail, AUTHORS for developers list.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* irc-send.c: implementation of IRC commands (client to server),
               according to RFC 1459,2810,2811,2812 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <netdb.h>
#include <time.h>
#include <sys/utsname.h>
#include <regex.h>

#include "../common/weechat.h"
#include "irc.h"
#include "../common/command.h"
#include "../common/util.h"
#include "../common/weeconfig.h"
#include "../gui/gui.h"


/*
 * irc_login: login to irc server
 */

void
irc_login (t_irc_server *server)
{
    char hostname[NI_MAXHOST];
    
    if ((server->password) && (server->password[0]))
        server_sendf (server, "PASS %s", server->password);
    
    gethostname (hostname, sizeof (hostname) - 1);
    hostname[sizeof (hostname) - 1] = '\0';
    if (!hostname[0])
        snprintf (hostname, NI_MAXHOST, "unknown");
    
    irc_display_prefix (server, server->buffer, PREFIX_INFO);
    gui_printf (server->buffer,
                _("%s: using hostname \"%s\"\n"),
                PACKAGE_NAME, hostname);
    if (!server->nick)
        server->nick = strdup (server->nick1);
    server_sendf (server,
                  "NICK %s\n"
                  "USER %s %s %s :%s",
                  server->nick, server->username, hostname, "servername",
                  server->realname);
    gui_input_draw (gui_current_window->buffer, 1);
}

/*
 * irc_cmd_send_admin: find information about the administrator of the server
 */

int
irc_cmd_send_admin (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "ADMIN %s", arguments);
    else
        server_sendf (server, "ADMIN");
    return 0;
}

/*
 * irc_send_me: send a ctcp action to a channel
 */

int
irc_send_me (t_irc_server *server, t_irc_channel *channel,
             char *arguments)
{
    char *string;
    
    server_sendf (server, "PRIVMSG %s :\01ACTION %s\01",
                  channel->name,
                  (arguments && arguments[0]) ? arguments : "");
    irc_display_prefix (NULL, channel->buffer, PREFIX_ACTION_ME);
    string = (arguments && arguments[0]) ?
        (char *)gui_color_decode ((unsigned char *)arguments, 1) : NULL;
    gui_printf (channel->buffer, "%s%s %s%s\n",
                GUI_COLOR(COLOR_WIN_CHAT_NICK),
                server->nick,
                GUI_COLOR(COLOR_WIN_CHAT),
                (string) ? string : "");
    if (string)
        free (string);
    return 0;
}

/*
 * irc_send_me_all_channels: send a ctcp action to all channels of a server
 */

int
irc_send_me_all_channels (t_irc_server *server, char *arguments)
{
    t_irc_channel *ptr_channel;
    
    for (ptr_channel = server->channels; ptr_channel;
         ptr_channel = ptr_channel->next_channel)
    {
        if (ptr_channel->type == CHANNEL_TYPE_CHANNEL)
            irc_send_me (server, ptr_channel, arguments);
    }
    return 0;
}

/*
 * irc_cmd_send_ame: send a ctcp action to all channels of all connected servers
 */

int
irc_cmd_send_ame (t_irc_server *server, t_irc_channel *channel,
                  char *arguments)
{
    t_irc_server *ptr_server;
    t_irc_channel *ptr_channel;
    
    /* make gcc happy */
    (void) server;
    (void) channel;
    
    gui_add_hotlist = 0;
    for (ptr_server = irc_servers; ptr_server;
         ptr_server = ptr_server->next_server)
    {
        if (ptr_server->is_connected)
        {
            for (ptr_channel = ptr_server->channels; ptr_channel;
                 ptr_channel = ptr_channel->next_channel)
            {
                if (ptr_channel->type == CHANNEL_TYPE_CHANNEL)
                    irc_send_me (ptr_server, ptr_channel, arguments);
            }
        }
    }
    gui_add_hotlist = 1;
    return 0;
}

/*
 * irc_cmd_send_amsg: send message to all channels of all connected servers
 */

int
irc_cmd_send_amsg (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    t_irc_server *ptr_server;
    t_irc_channel *ptr_channel;
    t_irc_nick *ptr_nick;
    char *string;
    
    /* make gcc happy */
    (void) server;
    (void) channel;
    
    if (arguments)
    {
        gui_add_hotlist = 0;
        for (ptr_server = irc_servers; ptr_server;
             ptr_server = ptr_server->next_server)
        {
            if (ptr_server->is_connected)
            {
                for (ptr_channel = ptr_server->channels; ptr_channel;
                     ptr_channel = ptr_channel->next_channel)
                {
                    if (ptr_channel->type == CHANNEL_TYPE_CHANNEL)
                    {
                        server_sendf (ptr_server, "PRIVMSG %s :%s",
                                      ptr_channel->name, arguments);
                        ptr_nick = nick_search (ptr_channel, ptr_server->nick);
                        if (ptr_nick)
                        {
                            irc_display_nick (ptr_channel->buffer, ptr_nick, NULL,
                                              MSG_TYPE_NICK, 1, -1, 0);
                            string = (char *)gui_color_decode ((unsigned char *)arguments, 1);
                            gui_printf (ptr_channel->buffer, "%s\n", (string) ? string : arguments);
                            if (string)
                                free (string);
                        }
                        else
                        {
                            irc_display_prefix (ptr_server, ptr_server->buffer, PREFIX_ERROR);
                            gui_printf (ptr_server->buffer,
                                        _("%s cannot find nick for sending message\n"),
                                        WEECHAT_ERROR);
                        }
                    }
                }
            }
        }
        gui_add_hotlist = 1;
    }
    else
        return -1;
    return 0;
}

/*
 * irc_send_away: toggle away status for one server
 */

void
irc_send_away (t_irc_server *server, char *arguments)
{
    char *string, buffer[4096];
    t_gui_window *ptr_window;
    time_t time_now, elapsed;
    
    if (arguments)
    {
        server->is_away = 1;
        if (server->away_message)
            free (server->away_message);
        server->away_message = (char *) malloc (strlen (arguments) + 1);
        if (server->away_message)
            strcpy (server->away_message, arguments);
        server->away_time = time (NULL);
        server_sendf (server, "AWAY :%s", arguments);
        if (cfg_irc_display_away != CFG_IRC_DISPLAY_AWAY_OFF)
        {
            string = (char *)gui_color_decode ((unsigned char *)arguments, 1);
            if (cfg_irc_display_away == CFG_IRC_DISPLAY_AWAY_LOCAL)
                irc_display_away (server, "away", (string) ? string : arguments);
            else
            {
                snprintf (buffer, sizeof (buffer), "is away: %s", (string) ? string : arguments);
                irc_send_me_all_channels (server, buffer);
            }
            if (string)
                free (string);
        }
        server_set_away (server, server->nick, 1);
        for (ptr_window = gui_windows; ptr_window;
             ptr_window = ptr_window->next_window)
        {
            if (SERVER(ptr_window->buffer) == server)
                ptr_window->buffer->last_read_line =
                    ptr_window->buffer->last_line;
        }
    }
    else
    {
        server_sendf (server, "AWAY");
        server->is_away = 0;
        if (server->away_message)
        {
            free (server->away_message);
            server->away_message = NULL;
        }
        if (server->away_time != 0)
        {
            time_now = time (NULL);
            elapsed = (time_now >= server->away_time) ?
                time_now - server->away_time : 0;
            server->away_time = 0;
            if (cfg_irc_display_away != CFG_IRC_DISPLAY_AWAY_OFF)
            {
                if (cfg_irc_display_away == CFG_IRC_DISPLAY_AWAY_LOCAL)
                {
                    snprintf (buffer, sizeof (buffer),
                              "gone %.2ld:%.2ld:%.2ld",
                              (long int)(elapsed / 3600),
                              (long int)((elapsed / 60) % 60),
                              (long int)(elapsed % 60));
                    irc_display_away (server, "back", buffer);
                }
                else
                {
                    snprintf (buffer, sizeof (buffer),
                              "is back (gone %.2ld:%.2ld:%.2ld)",
                              (long int)(elapsed / 3600),
                              (long int)((elapsed / 60) % 60),
                              (long int)(elapsed % 60));
                    irc_send_me_all_channels (server, buffer);
                }
            }
        }
        server_set_away (server, server->nick, 0);
    }
}

/*
 * irc_cmd_send_away: toggle away status
 */

int
irc_cmd_send_away (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    t_gui_buffer *buffer;
    char *pos;
    t_irc_server *ptr_server;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    /* make gcc happy */
    (void) channel;
    
    gui_add_hotlist = 0;
    if (arguments && (strncmp (arguments, "-all", 4) == 0))
    {
        pos = arguments + 4;
        while (pos[0] == ' ')
            pos++;
        if (!pos[0])
            pos = NULL;
        
        for (ptr_server = irc_servers; ptr_server;
             ptr_server = ptr_server->next_server)
        {
            if (ptr_server->is_connected)
                irc_send_away (ptr_server, pos);
        }
    }
    else
    {
        if (server && server->is_connected)
            irc_send_away (server, arguments);
        else
        {
            irc_display_prefix (NULL, NULL, PREFIX_ERROR);
            gui_printf_nolog (NULL,
                              _("%s command \"%s\" needs a server connection!\n"),
                              WEECHAT_ERROR, "away");
            return -1;
        }
    }
    
    gui_status_draw (buffer, 1);
    gui_add_hotlist = 1;
    return 0;
}

/*
 * irc_cmd_send_ban: bans nicks or hosts
 */

int
irc_cmd_send_ban (t_irc_server *server, t_irc_channel *channel,
                  char *arguments)
{
    t_gui_buffer *buffer;
    char *pos_channel, *pos, *pos2;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (arguments)
    {
        pos_channel = NULL;
        pos = strchr (arguments, ' ');
        if (pos)
        {
            pos[0] = '\0';
            
            if (string_is_channel (arguments))
            {
                pos_channel = arguments;
                pos++;
                while (pos[0] == ' ')
                    pos++;
            }
            else
            {
                pos[0] = ' ';
                pos = arguments;
            }
        }
        else
            pos = arguments;
        
        /* channel not given, use default buffer */
        if (!pos_channel)
        {
            if (!BUFFER_IS_CHANNEL(buffer))
            {
                irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
                gui_printf_nolog (server->buffer,
                                  _("%s \"%s\" command can only be executed in a channel buffer\n"),
                                  WEECHAT_ERROR, "ban");
                return -1;
            }
            pos_channel = CHANNEL(buffer)->name;
        }
        
        /* loop on users */
        while (pos && pos[0])
        {
            pos2 = strchr (pos, ' ');
            if (pos2)
            {
                pos2[0] = '\0';
                pos2++;
                while (pos2[0] == ' ')
                    pos2++;
            }
            server_sendf (server, "MODE %s +b %s", pos_channel, pos);
            pos = pos2;
        }
    }
    else
    {
        if (!BUFFER_IS_CHANNEL(buffer))
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s \"%s\" command can only be executed in a channel buffer\n"),
                              WEECHAT_ERROR, "ban");
            return -1;
        }
        server_sendf (server, "MODE %s +b", CHANNEL(buffer)->name);
    }
    
    return 0;
}

/*
 * irc_cmd_send_ctcp: send a ctcp message
 */

int
irc_cmd_send_ctcp (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    char *pos_type, *pos_args, *pos;
    struct timeval tv;
    
    /* make gcc happy */
    (void) channel;
    
    pos_type = strchr (arguments, ' ');
    if (pos_type)
    {
        pos_type[0] = '\0';
        pos_type++;
        while (pos_type[0] == ' ')
            pos_type++;
        pos_args = strchr (pos_type, ' ');
        if (pos_args)
        {
            pos_args[0] = '\0';
            pos_args++;
            while (pos_args[0] == ' ')
                pos_args++;
        }
        else
            pos_args = NULL;
        
        pos = pos_type;
        while (pos[0])
        {
            pos[0] = toupper (pos[0]);
            pos++;
        }

        irc_display_prefix (server, server->buffer, PREFIX_SERVER);
        gui_printf (server->buffer, "CTCP%s(%s%s%s)%s: %s%s",
                    GUI_COLOR(COLOR_WIN_CHAT_DARK),
                    GUI_COLOR(COLOR_WIN_CHAT_NICK),
                    arguments,
                    GUI_COLOR(COLOR_WIN_CHAT_DARK),
                    GUI_COLOR(COLOR_WIN_CHAT),
                    GUI_COLOR(COLOR_WIN_CHAT_CHANNEL),
                    pos_type);
        
        if ((ascii_strcasecmp (pos_type, "ping") == 0) && (!pos_args))
        {
            gettimeofday (&tv, NULL);
            server_sendf (server, "PRIVMSG %s :\01PING %d %d\01",
                          arguments, tv.tv_sec, tv.tv_usec);
            gui_printf (server->buffer, " %s%d %d\n",
                        GUI_COLOR(COLOR_WIN_CHAT),
                        tv.tv_sec, tv.tv_usec);
        }
        else
        {
            if (pos_args)
            {
                server_sendf (server, "PRIVMSG %s :\01%s %s\01",
                              arguments, pos_type, pos_args);
                gui_printf (server->buffer, " %s%s\n",
                            GUI_COLOR(COLOR_WIN_CHAT),
                            pos_args);
            }
            else
            {
                server_sendf (server, "PRIVMSG %s :\01%s\01",
                              arguments, pos_type);
                gui_printf (server->buffer, "\n");
            }
        }
    }
    return 0;
}

/*
 * irc_cmd_send_cycle: leave and rejoin a channel
 */

int
irc_cmd_send_cycle (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    t_gui_buffer *buffer;
    char *channel_name, *pos_args, *ptr_arg, *buf;
    t_irc_channel *ptr_channel;
    char **channels;
    int i, argc;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (arguments)
    {
        if (string_is_channel (arguments))
        {
            channel_name = arguments;
            pos_args = strchr (arguments, ' ');
            if (pos_args)
            {
                pos_args[0] = '\0';
                pos_args++;
                while (pos_args[0] == ' ')
                    pos_args++;
            }
            channels = explode_string (channel_name, ",", 0, &argc);
            if (channels)
            {
                for (i = 0; i < argc; i++)
                {
                    ptr_channel = channel_search (server, channels[i]);
                    /* mark channal as cycling */
                    if (ptr_channel &&
                        (ptr_channel->type == CHANNEL_TYPE_CHANNEL))
                        ptr_channel->cycle = 1;
                }
                free_exploded_string (channels);
            }
        }
        else
        {
            if (BUFFER_IS_SERVER(buffer))
            {
                irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
                gui_printf_nolog (server->buffer,
                                  _("%s \"%s\" command can not be executed on a server buffer\n"),
                                  WEECHAT_ERROR, "cycle");
                return -1;
            }
            
            /* does nothing on private buffer (cycle has no sense!) */
            if (BUFFER_IS_PRIVATE(buffer))
                return 0;
            
            channel_name = CHANNEL(buffer)->name;
            pos_args = arguments;
            CHANNEL(buffer)->cycle = 1;
        }
    }
    else
    {
        if (BUFFER_IS_SERVER(buffer))
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s \"%s\" command can not be executed on a server buffer\n"),
                              WEECHAT_ERROR, "part");
            return -1;
        }
        
        /* does nothing on private buffer (cycle has no sense!) */
        if (BUFFER_IS_PRIVATE(buffer))
            return 0;
        
        channel_name = CHANNEL(buffer)->name;
        pos_args = NULL;
        CHANNEL(buffer)->cycle = 1;
    }
    
    ptr_arg = (pos_args) ? pos_args :
              (cfg_irc_default_msg_part && cfg_irc_default_msg_part[0]) ?
              cfg_irc_default_msg_part : NULL;
    
    if (ptr_arg)
    {
        buf = weechat_strreplace (ptr_arg, "%v", PACKAGE_VERSION);
        server_sendf (server, "PART %s :%s", channel_name,
                      (buf) ? buf : ptr_arg);
        if (buf)
            free (buf);
    }
    else
        server_sendf (server, "PART %s", channel_name);
        
    return 0;
}

/*
 * irc_cmd_send_dehalfop: remove half operator privileges from nickname(s)
 */

int
irc_cmd_send_dehalfop (t_irc_server *server, t_irc_channel *channel,
                       int argc, char **argv)
{
    t_gui_buffer *buffer;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (BUFFER_IS_CHANNEL(buffer))
    {
        if (argc == 0)
            server_sendf (server, "MODE %s -h %s",
                          CHANNEL(buffer)->name,
                          server->nick);
        else
            irc_send_mode_nicks (server, CHANNEL(buffer)->name,
                                 "-", "h", argc, argv);
    }
    else
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s \"%s\" command can only be executed in a channel buffer\n"),
                          WEECHAT_ERROR, "dehalfop");
    }
    return 0;
}

/*
 * irc_cmd_send_deop: remove operator privileges from nickname(s)
 */

int
irc_cmd_send_deop (t_irc_server *server, t_irc_channel *channel,
                   int argc, char **argv)
{
    t_gui_buffer *buffer;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (BUFFER_IS_CHANNEL(buffer))
    {
        if (argc == 0)
            server_sendf (server, "MODE %s -o %s",
                          CHANNEL(buffer)->name,
                          server->nick);
        else
            irc_send_mode_nicks (server, CHANNEL(buffer)->name,
                                 "-", "o", argc, argv);
    }
    else
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s \"%s\" command can only be executed in a channel buffer\n"),
                          WEECHAT_ERROR, "deop");
    }
    return 0;
}

/*
 * irc_cmd_send_devoice: remove voice from nickname(s)
 */

int
irc_cmd_send_devoice (t_irc_server *server, t_irc_channel *channel,
                      int argc, char **argv)
{
    t_gui_buffer *buffer;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (BUFFER_IS_CHANNEL(buffer))
    {
        if (argc == 0)
            server_sendf (server, "MODE %s -v %s",
                          CHANNEL(buffer)->name,
                          server->nick);
        else
            irc_send_mode_nicks (server, CHANNEL(buffer)->name,
                                 "-", "v", argc, argv);
    }
    else
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s \"%s\" command can only be executed in a channel buffer\n"),
                          WEECHAT_ERROR, "devoice");
        return -1;
    }
    return 0;
}

/*
 * irc_cmd_send_die: shotdown the server
 */

int
irc_cmd_send_die (t_irc_server *server, t_irc_channel *channel,
                  char *arguments)
{
    /* make gcc happy */
    (void) channel;
    (void) arguments;
    
    server_sendf (server, "DIE");
    return 0;
}

/*
 * irc_cmd_send_halfop: give half operator privileges to nickname(s)
 */

int
irc_cmd_send_halfop (t_irc_server *server, t_irc_channel *channel,
                     int argc, char **argv)
{
    t_gui_buffer *buffer;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (BUFFER_IS_CHANNEL(buffer))
    {
        if (argc == 0)
            server_sendf (server, "MODE %s +h %s",
                          CHANNEL(buffer)->name,
                          server->nick);
        else
            irc_send_mode_nicks (server, CHANNEL(buffer)->name,
                                 "+", "h", argc, argv);
    }
    else
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s \"%s\" command can only be executed in a channel buffer\n"),
                          WEECHAT_ERROR, "halfop");
        return -1;
    }
    return 0;
}

/*
 * irc_cmd_send_info: get information describing the server
 */

int
irc_cmd_send_info (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "INFO %s", arguments);
    else
        server_sendf (server, "INFO");
    return 0;
}

/*
 * irc_cmd_send_invite: invite a nick on a channel
 */

int
irc_cmd_send_invite (t_irc_server *server, t_irc_channel *channel,
                     int argc, char **argv)
{
    t_gui_buffer *buffer;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (argc == 2)
        server_sendf (server, "INVITE %s %s", argv[0], argv[1]);
    else
    {
        if (!BUFFER_IS_CHANNEL(buffer))
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s \"%s\" command can only be executed in a channel buffer\n"),
                              WEECHAT_ERROR, "invite");
            return -1;
        }
        server_sendf (server, "INVITE %s %s",
                      argv[0], CHANNEL(buffer)->name);
    }
    return 0;
}

/*
 * irc_cmd_send_ison: check if a nickname is currently on IRC
 */

int
irc_cmd_send_ison (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "ISON %s", arguments);
    return 0;
}

/*
 * irc_cmd_send_join: join a new channel
 */

int
irc_cmd_send_join (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (string_is_channel (arguments))
        server_sendf (server, "JOIN %s", arguments);
    else
        server_sendf (server, "JOIN #%s", arguments);
    return 0;
}

/*
 * irc_cmd_send_kick: forcibly remove a user from a channel
 */

int
irc_cmd_send_kick (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    t_gui_buffer *buffer;
    char *pos_channel, *pos_nick, *pos_comment;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (string_is_channel (arguments))
    {
        pos_channel = arguments;
        pos_nick = strchr (arguments, ' ');
        if (!pos_nick)
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s wrong arguments for \"%s\" command\n"),
                              WEECHAT_ERROR, "kick");
            return -1;
        }
        pos_nick[0] = '\0';
        pos_nick++;
        while (pos_nick[0] == ' ')
            pos_nick++;
    }
    else
    {
        if (!BUFFER_IS_CHANNEL(buffer))
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s \"%s\" command can only be executed in a channel buffer\n"),
                              WEECHAT_ERROR, "kick");
            return -1;
        }
        pos_channel = CHANNEL(buffer)->name;
        pos_nick = arguments;
    }
    
    pos_comment = strchr (pos_nick, ' ');
    if (pos_comment)
    {
        pos_comment[0] = '\0';
        pos_comment++;
        while (pos_comment[0] == ' ')
            pos_comment++;
    }
    
    if (pos_comment)
        server_sendf (server, "KICK %s %s :%s", pos_channel, pos_nick, pos_comment);
    else
        server_sendf (server, "KICK %s %s", pos_channel, pos_nick);
    
    return 0;
}

/*
 * irc_cmd_send_kickban: forcibly remove a user from a channel and ban it
 */

int
irc_cmd_send_kickban (t_irc_server *server, t_irc_channel *channel,
                      char *arguments)
{
    t_gui_buffer *buffer;
    char *pos_channel, *pos_nick, *pos_comment;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (string_is_channel (arguments))
    {
        pos_channel = arguments;
        pos_nick = strchr (arguments, ' ');
        if (!pos_nick)
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s wrong arguments for \"%s\" command\n"),
                              WEECHAT_ERROR, "kickban");
            return -1;
        }
        pos_nick[0] = '\0';
        pos_nick++;
        while (pos_nick[0] == ' ')
            pos_nick++;
    }
    else
    {
        if (!BUFFER_IS_CHANNEL(buffer))
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s \"%s\" command can only be executed in a channel buffer\n"),
                              WEECHAT_ERROR, "kickban");
            return -1;
        }
        pos_channel = CHANNEL(buffer)->name;
        pos_nick = arguments;
    }
    
    pos_comment = strchr (pos_nick, ' ');
    if (pos_comment)
    {
        pos_comment[0] = '\0';
        pos_comment++;
        while (pos_comment[0] == ' ')
            pos_comment++;
    }
    
    server_sendf (server, "MODE %s +b %s", pos_channel, pos_nick);
    if (pos_comment)
        server_sendf (server, "KICK %s %s :%s", pos_channel, pos_nick, pos_comment);
    else
        server_sendf (server, "KICK %s %s", pos_channel, pos_nick);
    
    return 0;
}

/*
 * irc_cmd_send_kill: close client-server connection
 */

int
irc_cmd_send_kill (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    char *pos;
    
    /* make gcc happy */
    (void) channel;
    
    pos = strchr (arguments, ' ');
    if (pos)
    {
        pos[0] = '\0';
        pos++;
        while (pos[0] == ' ')
            pos++;
        server_sendf (server, "KILL %s :%s", arguments, pos);
    }
    else
        server_sendf (server, "KILL %s", arguments);
    return 0;
}

/*
 * irc_cmd_send_links: list all servernames which are known by the server
 *                     answering the query
 */

int
irc_cmd_send_links (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "LINKS %s", arguments);
    else
        server_sendf (server, "LINKS");
    return 0;
}

/*
 * irc_cmd_send_list: close client-server connection
 */

int
irc_cmd_send_list (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    char buffer[512];
    int ret;
    /* make gcc happy */
    (void) channel;
    
    if (server->cmd_list_regexp)
    {
	regfree (server->cmd_list_regexp);
	free (server->cmd_list_regexp);
	server->cmd_list_regexp = NULL;
    }
    
    if (arguments)
    {
	server->cmd_list_regexp = (regex_t *) malloc (sizeof (regex_t));
	if (server->cmd_list_regexp)
	{
	    if ((ret = regcomp (server->cmd_list_regexp, arguments, REG_NOSUB | REG_ICASE)) != 0)
	    {
		regerror (ret, server->cmd_list_regexp, buffer, sizeof(buffer));
		gui_printf (server->buffer,
				  _("%s \"%s\" is not a valid regular expression (%s)\n"),
				  WEECHAT_ERROR, arguments, buffer);
	    }
	    else
		server_sendf (server, "LIST");
	}
	else
	{
	    gui_printf (server->buffer,
			_("%s not enough memory for regular expression\n"),
			WEECHAT_ERROR);
	}
    }
    else
	server_sendf (server, "LIST");
    
    return 0;
}

/*
 * irc_cmd_send_lusers: get statistics about ths size of the IRC network
 */

int
irc_cmd_send_lusers (t_irc_server *server, t_irc_channel *channel,
                     char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "LUSERS %s", arguments);
    else
        server_sendf (server, "LUSERS");
    return 0;
}

/*
 * irc_cmd_send_me: send a ctcp action to the current channel
 */

int
irc_cmd_send_me (t_irc_server *server, t_irc_channel *channel,
                 char *arguments)
{
    t_gui_buffer *buffer;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (BUFFER_IS_SERVER(buffer))
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s \"%s\" command can not be executed on a server buffer\n"),
                          WEECHAT_ERROR, "me");
        return -1;
    }
    irc_send_me (server, CHANNEL(buffer), arguments);
    return 0;
}

/*
 * irc_cmd_send_mode: change mode for channel/nickname
 */

int
irc_cmd_send_mode (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "MODE %s", arguments);
    return 0;
}

/*
 * irc_send_mode_nicks: send mode change for many nicks on a channel
 */

void
irc_send_mode_nicks (t_irc_server *server, char *channel,
                     char *set, char *mode, int argc, char **argv)
{
    int i, length;
    char *command;
    
    length = 0;
    for (i = 0; i < argc; i++)
        length += strlen (argv[i]) + 1;
    length += strlen (channel) + (argc * strlen (mode)) + 32;
    command = (char *)malloc (length);
    if (command)
    {
        snprintf (command, length, "MODE %s %s", channel, set);
        for (i = 0; i < argc; i++)
            strcat (command, mode);
        for (i = 0; i < argc; i++)
        {
            strcat (command, " ");
            strcat (command, argv[i]);
        }
        server_sendf (server, "%s", command);
        free (command);
    }
}

/*
 * irc_cmd_send_motd: get the "Message Of The Day"
 */

int
irc_cmd_send_motd (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "MOTD %s", arguments);
    else
        server_sendf (server, "MOTD");
    return 0;
}

/*
 * irc_cmd_send_msg: send a message to a nick or channel
 */

int
irc_cmd_send_msg (t_irc_server *server, t_irc_channel *channel,
                  char *arguments)
{
    t_gui_window *window;
    t_gui_buffer *buffer;
    char *pos, *pos_comma;
    char *msg_pwd_hidden, *pos_pwd;
    t_irc_channel *ptr_channel;
    t_irc_nick *ptr_nick;
    char *string;
    
    irc_find_context (server, channel, &window, &buffer);
    
    pos = strchr (arguments, ' ');
    if (pos)
    {
        pos[0] = '\0';
        pos++;
        while (pos[0] == ' ')
            pos++;
        
        while (arguments && arguments[0])
        {
            pos_comma = strchr (arguments, ',');
            if (pos_comma)
            {
                pos_comma[0] = '\0';
                pos_comma++;
            }
            if (strcmp (arguments, "*") == 0)
            {
                if (!BUFFER_IS_CHANNEL(buffer) &&
                    !BUFFER_IS_PRIVATE(buffer))
                {
                    irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
                    gui_printf_nolog (server->buffer,
                                      _("%s \"%s\" command can only be executed in a channel or private buffer\n"),
                                      WEECHAT_ERROR, "msg *");
                    return -1;
                }
                ptr_channel = CHANNEL(buffer);
                if (BUFFER_IS_CHANNEL(buffer))
                    ptr_nick = nick_search (ptr_channel, server->nick);
                else
                    ptr_nick = NULL;
                irc_display_nick (buffer, ptr_nick,
                                  (ptr_nick) ? NULL : server->nick,
                                  MSG_TYPE_NICK, 1, -1, 0);
                string = (char *)gui_color_decode ((unsigned char *)pos, 1);
                gui_printf_type (buffer, MSG_TYPE_MSG, "%s\n",
                                 (string) ? string : "");
                if (string)
                    free (string);
                
                server_sendf (server, "PRIVMSG %s :%s", ptr_channel->name, pos);
            }
            else
            {
                if (string_is_channel (arguments))
                {
                    ptr_channel = channel_search (server, arguments);
                    if (ptr_channel)
                    {
                        ptr_nick = nick_search (ptr_channel, server->nick);
                        if (ptr_nick)
                        {
                            irc_display_nick (ptr_channel->buffer, ptr_nick, NULL,
                                              MSG_TYPE_NICK, 1, -1, 0);
                            string = (char *)gui_color_decode ((unsigned char *)pos, 1);
                            gui_printf_type (ptr_channel->buffer, MSG_TYPE_MSG, "%s\n",
                                             (string) ? string : "");
                            if (string)
                                free (string);
                        }
                        else
                        {
                            irc_display_prefix (server, server->buffer, PREFIX_ERROR);
                            gui_printf_nolog (server->buffer,
                                              _("%s nick \"%s\" not found for \"%s\" command\n"),
                                              WEECHAT_ERROR, server->nick, "msg");
                        }
                    }
                    server_sendf (server, "PRIVMSG %s :%s", arguments, pos);
                }
                else
                {
                    /* message to nickserv with identify ? */
                    if (strcmp (arguments, "nickserv") == 0)
                    {
                        msg_pwd_hidden = strdup (pos);
                        if (cfg_log_hide_nickserv_pwd)
                        {
                            pos_pwd = strstr (msg_pwd_hidden, "identify ");
                            if (!pos_pwd)
                                pos_pwd = strstr (msg_pwd_hidden, "register ");
                            if (pos_pwd)
                            {
                                pos_pwd += 9;
                                while (pos_pwd[0])
                                {
                                    pos_pwd[0] = '*';
                                    pos_pwd++;
                                }
                            }
                        }
                        irc_display_prefix (server, server->buffer, PREFIX_SERVER);
                        gui_printf_type (server->buffer, MSG_TYPE_NICK,
                                         "%s-%s%s%s- ",
                                         GUI_COLOR(COLOR_WIN_CHAT_DARK),
                                         GUI_COLOR(COLOR_WIN_CHAT_NICK),
                                         arguments,
                                         GUI_COLOR(COLOR_WIN_CHAT_DARK));
                        string = (char *)gui_color_decode ((unsigned char *)msg_pwd_hidden, 1);
                        gui_printf (server->buffer, "%s%s\n",
                                    GUI_COLOR(COLOR_WIN_CHAT),
                                    (string) ? string : "");
                        if (string)
                            free (string);
                        server_sendf (server, "PRIVMSG %s :%s", arguments, pos);
                        free (msg_pwd_hidden);
                        return 0;
                    }
                    
                    string = (char *)gui_color_decode ((unsigned char *)pos, 1);
                    ptr_channel = channel_search (server, arguments);
                    if (ptr_channel)
                    {
                        irc_display_nick (ptr_channel->buffer, NULL, server->nick,
                                          MSG_TYPE_NICK, 1, COLOR_WIN_NICK_SELF, 0);
                        gui_printf_type (ptr_channel->buffer, MSG_TYPE_MSG,
                                         "%s%s\n",
                                         GUI_COLOR(COLOR_WIN_CHAT),
                                         (string) ? string : "");
                    }
                    else
                    {
                        irc_display_prefix (server, server->buffer, PREFIX_SERVER);
                        gui_printf (server->buffer, "MSG%s(%s%s%s)%s: ",
                                    GUI_COLOR(COLOR_WIN_CHAT_DARK),
                                    GUI_COLOR(COLOR_WIN_CHAT_NICK),
                                    arguments,
                                    GUI_COLOR(COLOR_WIN_CHAT_DARK),
                                    GUI_COLOR(COLOR_WIN_CHAT));
                        gui_printf_type (server->buffer, MSG_TYPE_MSG,
                                         "%s\n",
                                         (string) ? string : pos);
                    }
                    if (string)
                        free (string);
                    server_sendf (server, "PRIVMSG %s :%s", arguments, pos);
                }
            }
            arguments = pos_comma;
        }
    }
    else
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s wrong argument count for \"%s\" command\n"),
                          WEECHAT_ERROR, "msg");
        return -1;
    }
    return 0;
}

/*
 * irc_cmd_send_names: list nicknames on channels
 */

int
irc_cmd_send_names (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    t_gui_buffer *buffer;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (arguments)
        server_sendf (server, "NAMES %s", arguments);
    else
    {
        if (!BUFFER_IS_CHANNEL(buffer))
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s \"%s\" command can only be executed in a channel buffer\n"),
                              WEECHAT_ERROR, "names");
            return -1;
        }
        else
            server_sendf (server, "NAMES %s",
                          CHANNEL(buffer)->name);
    }
    return 0;
}

/*
 * irc_cmd_send_nick_server: change nickname on a server
 */

void
irc_cmd_send_nick_server (t_irc_server *server, char *nickname)
{
    t_irc_channel *ptr_channel;
    
    if (server->is_connected)
        server_sendf (server, "NICK %s", nickname);
    else
    {
        if (server->nick)
            free (server->nick);
        server->nick = strdup (nickname);
        gui_input_draw (server->buffer, 1);
        for (ptr_channel = server->channels; ptr_channel;
             ptr_channel = ptr_channel->next_channel)
        {
            gui_input_draw (ptr_channel->buffer, 1);
        }
    }
}

/*
 * irc_cmd_send_nick: change nickname
 */

int
irc_cmd_send_nick (t_irc_server *server, t_irc_channel *channel,
                   int argc, char **argv)
{
    t_irc_server *ptr_server;
    
    /* make gcc happy */
    (void) channel;
    
    if (!server)
        return 0;
    
    if (argc == 2)
    {
        if (strncmp (argv[0], "-all", 4) != 0)
            return -1;
        
        for (ptr_server = irc_servers; ptr_server;
             ptr_server = ptr_server->next_server)
        {
            irc_cmd_send_nick_server (ptr_server, argv[1]);
        }
    }
    else
    {
        if (argc == 1)
            irc_cmd_send_nick_server (server, argv[0]);
        else
            return -1;
    }
    
    return 0;
}

/*
 * irc_cmd_send_notice: send notice message
 */

int
irc_cmd_send_notice (t_irc_server *server, t_irc_channel *channel,
                     char *arguments)
{
    char *pos, *string;
    
    /* make gcc happy */
    (void) channel;
    
    pos = strchr (arguments, ' ');
    if (pos)
    {
        pos[0] = '\0';
        pos++;
        while (pos[0] == ' ')
            pos++;
        irc_display_prefix (server, server->buffer, PREFIX_SERVER);
        string = (char *)gui_color_decode ((unsigned char *)pos, 1);
        gui_printf (server->buffer, "notice%s(%s%s%s)%s: %s\n",
                    GUI_COLOR(COLOR_WIN_CHAT_DARK),
                    GUI_COLOR(COLOR_WIN_CHAT_NICK),
                    arguments,
                    GUI_COLOR(COLOR_WIN_CHAT_DARK),
                    GUI_COLOR(COLOR_WIN_CHAT),
                    (string) ? string : "");
        if (string)
            free (string);
        server_sendf (server, "NOTICE %s :%s", arguments, pos);
    }
    else
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s wrong argument count for \"%s\" command\n"),
                          WEECHAT_ERROR, "notice");
        return -1;
    }
    return 0;
}

/*
 * irc_cmd_send_op: give operator privileges to nickname(s)
 */

int
irc_cmd_send_op (t_irc_server *server, t_irc_channel *channel,
                 int argc, char **argv)
{
    t_gui_buffer *buffer;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (BUFFER_IS_CHANNEL(buffer))
    {
        if (argc == 0)
            server_sendf (server, "MODE %s +o %s",
                          CHANNEL(buffer)->name,
                          server->nick);
        else
            irc_send_mode_nicks (server, CHANNEL(buffer)->name,
                                 "+", "o", argc, argv);
    }
    else
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s \"%s\" command can only be executed in a channel buffer\n"),
                          WEECHAT_ERROR, "op");
        return -1;
    }
    return 0;
}

/*
 * irc_cmd_send_oper: get oper privileges
 */

int
irc_cmd_send_oper (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "OPER %s", arguments);
    return 0;
}

/*
 * irc_cmd_send_part: leave a channel or close a private window
 */

int
irc_cmd_send_part (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    t_gui_buffer *buffer;
    char *channel_name, *pos_args, *ptr_arg, *buf;
    t_irc_channel *ptr_channel;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (arguments)
    {
        if (string_is_channel (arguments))
        {
            channel_name = arguments;
            pos_args = strchr (arguments, ' ');
            if (pos_args)
            {
                pos_args[0] = '\0';
                pos_args++;
                while (pos_args[0] == ' ')
                    pos_args++;
            }
        }
        else
        {
            if (!CHANNEL(buffer))
            {
                irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
                gui_printf_nolog (server->buffer,
                                  _("%s \"%s\" command can only be executed in a channel or private buffer\n"),
                                  WEECHAT_ERROR, "part");
                return -1;
            }
            channel_name = CHANNEL(buffer)->name;
            pos_args = arguments;
        }
    }
    else
    {
        if (!CHANNEL(buffer))
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s \"%s\" command can only be executed in a channel or private buffer\n"),
                              WEECHAT_ERROR, "part");
            return -1;
        }
        if (BUFFER_IS_PRIVATE(buffer))
        {
            ptr_channel = CHANNEL(buffer);
            gui_buffer_free (ptr_channel->buffer, 1);
            channel_free (server, ptr_channel);
            gui_status_draw (buffer, 1);
            gui_input_draw (buffer, 1);
            return 0;
        }
        channel_name = CHANNEL(buffer)->name;
        pos_args = NULL;
    }
    
    ptr_arg = (pos_args) ? pos_args :
              (cfg_irc_default_msg_part && cfg_irc_default_msg_part[0]) ?
              cfg_irc_default_msg_part : NULL;
    
    if (ptr_arg)
    {
        buf = weechat_strreplace (ptr_arg, "%v", PACKAGE_VERSION);
        server_sendf (server, "PART %s :%s", channel_name,
                      (buf) ? buf : ptr_arg);
        if (buf)
            free (buf);
    }
    else
        server_sendf (server, "PART %s", channel_name);
        
    return 0;
}

/*
 * irc_cmd_send_ping: ping a server
 */

int
irc_cmd_send_ping (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "PING %s", arguments);
    return 0;
}

/*
 * irc_cmd_send_pong: send pong answer to a daemon
 */

int
irc_cmd_send_pong (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "PONG %s", arguments);
    return 0;
}

/*
 * irc_cmd_send_query: start private conversation with a nick
 */

int
irc_cmd_send_query (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    t_gui_window *window;
    t_gui_buffer *buffer;
    char *pos, *string;
    t_irc_channel *ptr_channel;
    t_gui_buffer *ptr_buffer;
    
    irc_find_context (server, channel, &window, &buffer);
    
    pos = strchr (arguments, ' ');
    if (pos)
    {
        pos[0] = '\0';
        pos++;
        while (pos[0] == ' ')
            pos++;
        if (!pos[0])
            pos = NULL;
    }
    
    /* create private window if not already opened */
    ptr_channel = channel_search (server, arguments);
    if (!ptr_channel)
    {
        ptr_channel = channel_new (server, CHANNEL_TYPE_PRIVATE, arguments);
        if (!ptr_channel)
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s cannot create new private buffer \"%s\"\n"),
                              WEECHAT_ERROR, arguments);
            return -1;
        }
        gui_buffer_new (window, server, ptr_channel,
                        BUFFER_TYPE_STANDARD, 1);
        gui_chat_draw_title (ptr_channel->buffer, 1);
    }
    else
    {
        for (ptr_buffer = gui_buffers; ptr_buffer; ptr_buffer = ptr_buffer->next_buffer)
        {
            if (ptr_buffer->channel == ptr_channel)
            {
                gui_window_switch_to_buffer (window, ptr_buffer);
                gui_window_redraw_buffer (ptr_buffer);
                break;
            }
        }
    }
    
    /* display text if given */
    if (pos)
    {
        irc_display_nick (ptr_channel->buffer, NULL, server->nick,
                          MSG_TYPE_NICK, 1, COLOR_WIN_NICK_SELF, 0);
        string = (char *)gui_color_decode ((unsigned char *)pos, 1);
        gui_printf_type (ptr_channel->buffer, MSG_TYPE_MSG,
                         "%s%s\n",
                         GUI_COLOR(COLOR_WIN_CHAT),
                         (string) ? string : "");
        if (string)
            free (string);
        server_sendf (server, "PRIVMSG %s :%s", arguments, pos);
    }
    return 0;
}

/*
 * irc_send_quit_server: send QUIT to a server
 */

void
irc_send_quit_server (t_irc_server *server, char *arguments)
{
    char *ptr_arg, *buf;
    
    if (server->is_connected)
    {
        ptr_arg = (arguments) ? arguments :
            (cfg_irc_default_msg_quit && cfg_irc_default_msg_quit[0]) ?
            cfg_irc_default_msg_quit : NULL;
        
        if (ptr_arg)
        {
            buf = weechat_strreplace (ptr_arg, "%v", PACKAGE_VERSION);
            server_sendf (server, "QUIT :%s",
                          (buf) ? buf : ptr_arg);
            if (buf)
                free (buf);
        }
        else
            server_sendf (server, "QUIT");
    }
}

/*
 * irc_cmd_send_quit: disconnect from all servers and quit WeeChat
 */

int
irc_cmd_send_quit (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    t_irc_server *ptr_server;
    
    /* make gcc happy */
    (void) server;
    (void) channel;
    
    for (ptr_server = irc_servers; ptr_server;
         ptr_server = ptr_server->next_server)
    {
        irc_send_quit_server (ptr_server, arguments);
    }
    quit_weechat = 1;
    return 0;
}

/*
 * irc_cmd_send_quote: send raw data to server
 */

int
irc_cmd_send_quote (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "%s", arguments);
    return 0;
}

/*
 * irc_cmd_send_rehash: tell the server to reload its config file
 */

int
irc_cmd_send_rehash (t_irc_server *server, t_irc_channel *channel,
                     char *arguments)
{
    /* make gcc happy */
    (void) channel;
    (void) arguments;
    
    server_sendf (server, "REHASH");
    return 0;
}

/*
 * irc_cmd_send_restart: tell the server to restart itself
 */

int
irc_cmd_send_restart (t_irc_server *server, t_irc_channel *channel,
                      char *arguments)
{
    /* make gcc happy */
    (void) channel;
    (void) arguments;
    
    server_sendf (server, "RESTART");
    return 0;
}

/*
 * irc_cmd_send_service: register a new service
 */

int
irc_cmd_send_service (t_irc_server *server, t_irc_channel *channel,
                      char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "SERVICE %s", arguments);
    return 0;
}

/*
 * irc_cmd_send_servlist: list services currently connected to the network
 */

int
irc_cmd_send_servlist (t_irc_server *server, t_irc_channel *channel,
                       char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "SERVLIST %s", arguments);
    else
        server_sendf (server, "SERVLIST");
    return 0;
}

/*
 * irc_cmd_send_squery: deliver a message to a service
 */

int
irc_cmd_send_squery (t_irc_server *server, t_irc_channel *channel,
                     char *arguments)
{
    char *pos;
    
    /* make gcc happy */
    (void) channel;

    pos = strchr (arguments, ' ');
    if (pos)
    {
        pos[0] = '\0';
        pos++;
        while (pos[0] == ' ')
        {
            pos++;
        }
        server_sendf (server, "SQUERY %s :%s", arguments, pos);
    }
    else
        server_sendf (server, "SQUERY %s", arguments);
    
    return 0;
}

/*
 * irc_cmd_send_squit: disconnect server links
 */

int
irc_cmd_send_squit (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "SQUIT %s", arguments);
    return 0;
}

/*
 * irc_cmd_send_stats: query statistics about server
 */

int
irc_cmd_send_stats (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "STATS %s", arguments);
    else
        server_sendf (server, "STATS");
    return 0;
}

/*
 * irc_cmd_send_summon: give users who are on a host running an IRC server
 *                      a message asking them to please join IRC
 */

int
irc_cmd_send_summon (t_irc_server *server, t_irc_channel *channel,
                     char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "SUMMON %s", arguments);
    return 0;
}

/*
 * irc_cmd_send_time: query local time from server
 */

int
irc_cmd_send_time (t_irc_server *server, t_irc_channel *channel,
                   char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "TIME %s", arguments);
    else
        server_sendf (server, "TIME");
    return 0;
}

/*
 * irc_cmd_send_topic: get/set topic for a channel
 */

int
irc_cmd_send_topic (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    t_gui_buffer *buffer;
    char *channel_name, *new_topic, *pos;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    channel_name = NULL;
    new_topic = NULL;
    
    if (arguments)
    {
        if (string_is_channel (arguments))
        {
            channel_name = arguments;
            pos = strchr (arguments, ' ');
            if (pos)
            {
                pos[0] = '\0';
                pos++;
                while (pos[0] == ' ')
                    pos++;
                new_topic = (pos[0]) ? pos : NULL;
            }
        }
        else
            new_topic = arguments;
    }
    
    /* look for current channel if not specified */
    if (!channel_name)
    {
        if (BUFFER_IS_SERVER(buffer))
        {
            irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
            gui_printf_nolog (server->buffer,
                              _("%s \"%s\" command can not be executed on a server buffer\n"),
                              WEECHAT_ERROR, "topic");
            return -1;
        }
        channel_name = CHANNEL(buffer)->name;
    }
    
    if (new_topic)
    {
        if (strcmp (new_topic, "-delete") == 0)
            server_sendf (server, "TOPIC %s :", channel_name);
        else
            server_sendf (server, "TOPIC %s :%s", channel_name, new_topic);
    }
    else
        server_sendf (server, "TOPIC %s", channel_name);
    
    return 0;
}

/*
 * irc_cmd_send_trace: find the route to specific server
 */

int
irc_cmd_send_trace (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "TRACE %s", arguments);
    else
        server_sendf (server, "TRACE");
    return 0;
}

/*
 * irc_cmd_send_unban: unbans nicks or hosts
 */

int
irc_cmd_send_unban (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    t_gui_buffer *buffer;
    char *pos_channel, *pos, *pos2;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (arguments)
    {
        pos_channel = NULL;
        pos = strchr (arguments, ' ');
        if (pos)
        {
            pos[0] = '\0';
            
            if (string_is_channel (arguments))
            {
                pos_channel = arguments;
                pos++;
                while (pos[0] == ' ')
                    pos++;
            }
            else
            {
                pos[0] = ' ';
                pos = arguments;
            }
        }
        else
            pos = arguments;
        
        /* channel not given, use default buffer */
        if (!pos_channel)
        {
            if (!BUFFER_IS_CHANNEL(buffer))
            {
                irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
                gui_printf_nolog (server->buffer,
                                  _("%s \"%s\" command can only be executed in a channel buffer\n"),
                                  WEECHAT_ERROR, "unban");
                return -1;
            }
            pos_channel = CHANNEL(buffer)->name;
        }
        
        /* loop on users */
        while (pos && pos[0])
        {
            pos2 = strchr (pos, ' ');
            if (pos2)
            {
                pos2[0] = '\0';
                pos2++;
                while (pos2[0] == ' ')
                    pos2++;
            }
            server_sendf (server, "MODE %s -b %s", pos_channel, pos);
            pos = pos2;
        }
    }
    else
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s wrong argument count for \"%s\" command\n"),
                          WEECHAT_ERROR, "unban");
        return -1;
    }
    return 0;
}

/*
 * irc_cmd_send_userhost: return a list of information about nicknames
 */

int
irc_cmd_send_userhost (t_irc_server *server, t_irc_channel *channel,
                       char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "USERHOST %s", arguments);
    return 0;
}

/*
 * irc_cmd_send_users: list of users logged into the server
 */

int
irc_cmd_send_users (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "USERS %s", arguments);
    else
        server_sendf (server, "USERS");
    return 0;
}

/*
 * irc_cmd_send_version: gives the version info of nick or server (current or specified)
 */

int
irc_cmd_send_version (t_irc_server *server, t_irc_channel *channel,
                      char *arguments)
{
    t_gui_buffer *buffer;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (arguments)
    {
        if (BUFFER_IS_CHANNEL(buffer) &&
            nick_search (CHANNEL(buffer), arguments))
            server_sendf (server, "PRIVMSG %s :\01VERSION\01",
                          arguments);
        else
            server_sendf (server, "VERSION %s",
                          arguments);
    }
    else
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_INFO);
        gui_printf (server->buffer, _("%s, compiled on %s %s\n"),
                    PACKAGE_STRING,
                    __DATE__, __TIME__);
        server_sendf (server, "VERSION");
    }
    return 0;
}

/*
 * irc_cmd_send_voice: give voice to nickname(s)
 */

int
irc_cmd_send_voice (t_irc_server *server, t_irc_channel *channel,
                    int argc, char **argv)
{
    t_gui_buffer *buffer;
    
    irc_find_context (server, channel, NULL, &buffer);
    
    if (BUFFER_IS_CHANNEL(buffer))
    {
        if (argc == 0)
            server_sendf (server, "MODE %s +v %s",
                          CHANNEL(buffer)->name,
                          server->nick);
        else
            irc_send_mode_nicks (server, CHANNEL(buffer)->name,
                                 "+", "v", argc, argv);
    }
    else
    {
        irc_display_prefix (NULL, server->buffer, PREFIX_ERROR);
        gui_printf_nolog (server->buffer,
                          _("%s \"%s\" command can only be executed in a channel buffer\n"),
                          WEECHAT_ERROR, "voice");
        return -1;
    }
    return 0;
}

/*
 * irc_cmd_send_wallops: send a message to all currently connected users who
 *                       have set the 'w' user mode for themselves
 */

int
irc_cmd_send_wallops (t_irc_server *server, t_irc_channel *channel,
                      char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "WALLOPS :%s", arguments);
    return 0;
}

/*
 * irc_cmd_send_who: generate a query which returns a list of information
 */

int
irc_cmd_send_who (t_irc_server *server, t_irc_channel *channel,
                  char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    if (arguments)
        server_sendf (server, "WHO %s", arguments);
    else
        server_sendf (server, "WHO");
    return 0;
}

/*
 * irc_cmd_send_whois: query information about user(s)
 */

int
irc_cmd_send_whois (t_irc_server *server, t_irc_channel *channel,
                    char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "WHOIS %s", arguments);
    return 0;
}

/*
 * irc_cmd_send_whowas: ask for information about a nickname which no longer exists
 */

int
irc_cmd_send_whowas (t_irc_server *server, t_irc_channel *channel,
                     char *arguments)
{
    /* make gcc happy */
    (void) channel;
    
    server_sendf (server, "WHOWAS %s", arguments);
    return 0;
}
