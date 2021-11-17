/// <summary>
/// @file agent-shell-session.c
/// @author {Do Huy Hoang} ({huyhoangdo0205@gmail.com})
/// </summary>
/// @version 1.0
/// @date 2021-09-24
/// 
/// @copyright Copyright (c) 2021
/// 
#include <agent-shell-session.h>
#include <agent-type.h>
#include <agent-message.h>
#include <agent-child-process.h>
#include <agent-socket.h>


#include <logging.h>
#include <general-constant.h>
#include <message-form.h>
#include <error-code.h>
#include <agent-object.h>
#include <child-process-constant.h>
#include <child-process-resources-assign.h>



   
struct _ShellSession
{
    ChildProcess* process;

    gchar* script_file;

    gchar* output_file;

    gchar* shell_script;
    
    gint model_id;
    
    gint id;
};


static ShellSession shell_session_pool[MAX_POWERSHELL_INSTANCE] = {0};


gchar*
shell_session_get_output(gint process_id)
{
    GError* error = NULL;
    gchar* shell_script;
    g_file_get_contents(shell_session_pool[process_id].output_file,
        &shell_script,NULL, &error);
    if(error)
    {
        return NULL;
    }

    return shell_script;
}


gchar*
shell_session_get_script(gint process_id)
{
    GError* error = NULL;
    gchar* shell_script;
    g_file_get_contents(shell_session_pool[process_id].script_file,
        &shell_script,NULL, &error);
    if(error != NULL)
        return NULL;

    return shell_script;
}

gint
shell_session_get_id(gint process_id)
{
    return shell_session_pool[process_id].id;
}

gint
shell_session_get_model(gint process_id)
{
    return shell_session_pool[process_id].model_id;
}





static ShellSession* 
get_shell_session(gint process_id)
{
    return &(shell_session_pool[process_id]);
}

void
shell_output_handle(GBytes* data,
    gint process_id,
    AgentObject* agent)
{
    return;
}


void
shell_process_handle(ChildProcess* proc,
                    DWORD exit_code,
                    AgentObject* agent)
{
    if(exit_code == STILL_ACTIVE)
    {
        return;
    }
    else
    {
        gint id = get_child_process_id(proc);
        agent_on_shell_process_terminate(agent, id);
        close_child_process(proc);
    }
}



static void
create_new_shell_process(AgentObject* agent, 
                         ShellSession* session)
{
    GString* string = g_string_new(session->script_file);
    g_string_append(string, " | Out-File -Encoding UTF8 ");
    g_string_append(string, session->output_file);
    gchar* command = g_string_free(string,FALSE);

    gint childprocess_id = get_child_process_id(session->process);

    create_new_child_process(
        POWERSHELL_BINARY, 
            childprocess_id, command,
            shell_output_handle,
            shell_process_handle, agent);
    g_free(command);
}

static void 
write_to_script_file(AgentObject* agent, ShellSession* session)
{
    GFile* file = g_file_parse_name(session->script_file);

    if(!g_file_replace_contents(file, session->shell_script,strlen(session->shell_script),
        NULL,FALSE,G_FILE_CREATE_REPLACE_DESTINATION,NULL,NULL, NULL,NULL))
    {
        agent_report_error(agent,ERROR_FILE_OPERATION);					
    }

    return;
}


typedef struct _ShellSessionModel
{
    gchar string_model[500];

    gchar* script;

    gint model_id;
}ShellSessionModel;

static ShellSessionModel model[10];


void
initialize_shell_session(AgentObject* agent,
                         gchar* data_string)
{
    ShellSession* session;
    GError* error = NULL;
    return;

    for(gint i = 0; i< 10;i++)
    {
        if(!g_strcmp0(data_string,model[i].string_model))
        {
            ChildProcess* process = get_available_child_process();
            gint process_id = get_child_process_id(process);

            session = get_shell_session(process_id);
            session->script_file = GET_SHELL_SCRIPT_FILE(process_id);
            session->output_file = GET_SHELL_OUTPUT_FILE(process_id);
            session->model_id = model[i].model_id;
            session->shell_script = model[i].script;
            GRand* random = g_rand_new();
            session->id = g_rand_int(random);
            write_to_script_file(agent, session);
            create_new_shell_process(agent, session);
            return;
        }
    }


    JsonParser* parser = json_parser_new();
    Message* json_data = get_json_object_from_string(data_string,&error,parser);
    if(!error == NULL) { g_object_unref(parser); return; }


    ChildProcess* process = get_available_child_process();
    gint process_id = get_child_process_id(process);

    session = get_shell_session(process_id);
    session->script_file = GET_SHELL_SCRIPT_FILE(process_id);
    session->output_file = GET_SHELL_OUTPUT_FILE(process_id);
    session->shell_script = json_object_get_string_member(json_data,"Script");
    session->model_id = json_object_get_int_member(json_data,"ModelID");
    session->id = json_object_get_int_member(json_data,"ID");
    session->process = process;

    for(int i = 0; i< 10;i++)
    {
        if(!(model[i].string_model))
        {
            model[i].script=    session->shell_script;
            model[i].model_id = session->model_id;
            memcpy(model[i].string_model,data_string,strlen(data_string));
        }
    }

    write_to_script_file(agent, session);
    create_new_shell_process(agent, session);
    g_object_unref(parser);    
    return;
}

static void
character_remover(gchar** string, gchar* character)
{
    char **split = g_strsplit(*string, character, -1);
    memset(*string,0,strlen(*string));
    *string = g_strjoinv("", split);
    g_strfreev(split);
}

void
report_shell_session(AgentObject* agent,
                    gint process_id)
{
    gchar* script = shell_session_get_script(process_id);
    gchar* output = shell_session_get_output(process_id);
    gint id =       shell_session_get_id(process_id);
    gint model =    shell_session_get_model(process_id);
    if(script == NULL || output == NULL) { agent_report_error(agent, "fail to get script output"); return; }

    gchar* temp = malloc(strlen(output));
    memcpy(temp,output+3,strlen(output));

    character_remover(&temp, "\n");
    character_remover(&temp, "\r");


    guint status;
    SoupMessage *msg;
    const char * mycontent; //alloc and fill this with your data
    static SoupSession* session;
    const gchar* https_aliases[] = { "https", NULL };
    Socket* socket = agent_get_socket(agent);

    session = 
        soup_session_new_with_options(
            SOUP_SESSION_SSL_STRICT, FALSE,
            SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, TRUE,
            //SOUP_SESSION_SSL_CA_FILE, "/etc/ssl/certs/ca-bundle.crt",
            SOUP_SESSION_HTTPS_ALIASES, https_aliases, NULL);


    GString* uri = g_string_new(socket_get_host_url(socket));
    g_string_append(uri,"?ModelID=");
    gchar* buffer;
    itoa(id,buffer,10);
    g_string_append(uri,buffer);
    g_string_append(uri,"&ID=");

    msg = soup_message_new ("POST", "https://host.thinkmay.net/Account/Login?");
    gchar* parammeter = "";
    soup_message_set_request (msg, "application/json",
            SOUP_MEMORY_COPY, script, strlen (script));
        

    status = soup_session_send_message (session, msg);

    if(status != 200)
    {
        agent_report_error(agent, "Unable to update script output");
    }
}