#include <agent-session-initializer.h>
#include <agent-socket.h>
#include <agent-type.h>
#include <agent-child-process.h>
#include <state-indicator.h>

#include <error-code.h>
#include <message-form.h>
#include <general-constant.h>
#include <global-var.h>
#include <logging.h>

#include <gmodule.h>
#include <Windows.h>
#include <stdio.h>

#define BUFFER_SIZE 10000


struct _RemoteSession
{
    ChildProcess* process;

    gchar session_core_url[50];

    SoupSession* session;
};


RemoteSession*
intialize_remote_session_service()
{
    RemoteSession* remote = malloc(sizeof(RemoteSession));
    GString* base_url = g_string_new("http://localhost:");
    g_string_append(base_url,SESSION_CORE_PORT);
    g_string_append(base_url,"/agent");
    gchar* url = g_string_free(base_url,FALSE);

    const gchar* http_aliases[] = { "http", NULL };
    remote->session = soup_session_new_with_options(
            SOUP_SESSION_SSL_STRICT, FALSE,
            SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, TRUE,
            SOUP_SESSION_HTTPS_ALIASES, http_aliases, NULL);

    remote->process = NULL;
    memset(remote->session_core_url,0,50);
    memcpy(remote->session_core_url,url,strlen(url));
    return remote;
}


static void
handler_session_core_state_function(ChildProcess* proc,
                                    AgentServer* agent)
{
    RemoteSession* session = agent_get_remote_session(agent);

    SoupMessage* message = soup_message_new(SOUP_METHOD_POST,session->session_core_url);
    soup_message_headers_append(message->request_headers,"Authorization",TOKEN);

    SoupMessageBody* body = message->request_body;
    
    soup_session_send(session->session,message,NULL,NULL);
}


static void
handle_session_core_error(GBytes* buffer,
    AgentServer* agent,
    gpointer data)
{
    gchar* message = g_bytes_get_data(buffer, NULL);
    worker_log_output(message);
}

static void
handle_session_core_output(GBytes* buffer,
    AgentServer* agent,
    gpointer data)
{
    gchar* message = g_bytes_get_data(buffer, NULL);
    worker_log_output(message);
}

gboolean
session_reconnect(AgentServer* agent)
{
    RemoteSession* session = agent_get_remote_session(agent);
    if(session->process)
        return FALSE;

    GString* core_script = g_string_new(SESSION_CORE_BINARY);
    g_string_append(core_script," --token=");
    g_string_append(core_script,TOKEN);
    g_string_append(core_script," --clusterip=");
    g_string_append(core_script,CLUSTER_IP);


    session->process =
    create_new_child_process(g_string_free(core_script,FALSE),
        (ChildStdErrHandle)handle_session_core_error,
        (ChildStdOutHandle)handle_session_core_output,
        (ChildStateHandle)handler_session_core_state_function, agent,NULL);

    if(!session->process)
        return FALSE;
    else    
        return TRUE;
}

gboolean
session_disconnect(AgentServer* agent)
{
    RemoteSession* session = agent_get_remote_session(agent);
    if(!session->process)
        return FALSE;
    childprocess_force_exit(session->process);
    clean_childprocess(session->process);
    session->process = NULL;
}

gboolean
session_terminate(AgentServer* agent)
{
    RemoteSession* session = agent_get_remote_session(agent);
    if(!session->process)
        return FALSE;

    childprocess_force_exit(session->process);
    clean_childprocess(session->process);
    session->process = NULL;
}

gboolean
session_initialize(AgentServer* agent)
{
    RemoteSession* session = agent_get_remote_session(agent);
    if(session->process)
        return FALSE;

    GString* core_script = g_string_new(SESSION_CORE_BINARY);
    g_string_append(core_script," --token=");
    g_string_append(core_script,TOKEN);
    g_string_append(core_script," --clusterip=");
    g_string_append(core_script,CLUSTER_IP);

    session->process =
    create_new_child_process(g_string_free(core_script,FALSE),
        (ChildStdErrHandle)handle_session_core_error,
        (ChildStdOutHandle)handle_session_core_output,
        (ChildStateHandle)handler_session_core_state_function, agent,NULL);
    
    if(!session->process)
        return FALSE;
    else    
        return TRUE;
}

gboolean
send_message_to_core(AgentServer* agent, gchar* buffer)
{
    RemoteSession* session = agent_get_remote_session(agent);

    SoupMessage* message = soup_message_new(SOUP_METHOD_POST,session->session_core_url);
    soup_message_headers_append(message->request_headers,"Authorization",TOKEN);

    soup_message_set_request(message,"application/text",SOUP_MEMORY_COPY,buffer,strlen(buffer)); 
    soup_session_send_async(session->session,message,NULL,NULL,NULL);
}