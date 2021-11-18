#include <agent-state-unregistered.h>
#include <agent-state.h>
#include <glib.h>
#include <agent-session-initializer.h>
#include <agent-state-on-session.h>
#include <agent-socket.h>
#include <agent-shell-session.h>

#include <state-indicator.h>
#include <general-constant.h>
#include <message-form.h>
#include <error-code.h>
#include <logging.h>

static void
on_open_session_initialize(AgentServer* agent)
{
    AgentState* open_state = transition_to_on_session_state();
    agent_set_state(agent, open_state);
    session_initialize(agent);
}


static void
open_state_send_message_to_host(AgentServer* agent,
    gchar* message)
{
    static gboolean initialized = FALSE;
    static gint SlaveID;
    if(!initialized)
    {
        JsonParser* parser = json_parser_new();
        GError* error = NULL;
        json_parser_load_from_file(parser, HOST_CONFIG_FILE,&error);
        if(!error == NULL)
        {
            agent_report_error(agent, ERROR_FILE_OPERATION);
        }
        JsonNode* root = json_parser_get_root(parser);
        JsonObject* obj = json_node_get_object(root);
        SlaveID = json_object_get_int_member(obj,DEVICE_ID);
        initialized = TRUE;
    }

    GError* error = NULL;
    JsonParser* parser = json_parser_new();
    Message* object = get_json_object_from_string(message,&error,parser);
    if (!error == NULL || object == NULL) { return; }


    json_object_set_int_member(object,
        DEVICE_ID, SlaveID);

    gchar* string = get_string_from_json_object(object);
    send_message_to_host(agent,string);
    g_object_unref(parser);
    g_free(string);
}

static gchar*
open_get_state(void)
{
    return AGENT_OPEN;
}




static void
open_on_shell_process_exit(AgentServer* agent, 
                           gint process_id)
{
    report_shell_session(agent, process_id);
}


AgentState*
transition_to_on_open_state(void)
{
    static AgentState open_state;
    static gboolean initialized = FALSE;

    if(!initialized)
    {
        default_method(&open_state);
        open_state.session_initialize = on_open_session_initialize;
        open_state.send_message_to_host = open_state_send_message_to_host;
        open_state.get_current_state = open_get_state;  
        open_state.on_shell_process_exit = open_on_shell_process_exit;


        initialized = TRUE; 
    }
    write_to_log_file(AGENT_GENERAL_LOG, open_get_state());    
    return &open_state;
}



