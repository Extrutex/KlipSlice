#ifndef __NETWORK_Agent_HPP__
#define __NETWORK_Agent_HPP__

#include "bambu_networking.hpp"
#include "libslic3r/ProjectTask.hpp"
#include "ICloudServiceAgent.hpp"
#include "IPrinterAgent.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Slic3r {

// The NetworkAgent class
class NetworkAgent
{
public:
    // Sub-agent composition constructor (uses injected sub-agents)
    NetworkAgent(std::shared_ptr<ICloudServiceAgent> cloud_agent,
                 std::shared_ptr<IPrinterAgent> printer_agent);

    ~NetworkAgent();

    // Sub-agent accessors
    std::shared_ptr<ICloudServiceAgent> get_cloud_agent(const std::string& provider = ORCA_CLOUD_PROVIDER) const;
    std::shared_ptr<IPrinterAgent> get_printer_agent() const { return m_printer_agent; }

    // Shared agent management
    void add_cloud_agent(const std::string& provider, std::shared_ptr<ICloudServiceAgent> agent);
    void set_printer_agent(std::shared_ptr<IPrinterAgent> printer_agent);
    int set_queue_on_main_fn(QueueOnMainFn fn, const std::string& provider = ORCA_CLOUD_PROVIDER);

    // Cloud agent methods
    // These methods will be forwarded to all cloud agents
    int init_log();
    int set_config_dir(std::string config_dir);
    int set_cert_file(std::string folder, std::string filename);
    int set_country_code(std::string country_code);
    int start();
    int set_on_server_connected_fn(AppOnServerConnectedFn fn);
    int set_on_http_error_fn(AppOnHttpErrorFn fn);
    int set_get_country_code_fn(GetCountryCodeFn fn);
    int connect_server();

    int change_user(std::string user_info, const std::string& provider = ORCA_CLOUD_PROVIDER);
    bool is_user_login(const std::string& provider = ORCA_CLOUD_PROVIDER);
    int user_logout(bool request = false, const std::string& provider = ORCA_CLOUD_PROVIDER);
    std::string get_user_id(const std::string& provider = ORCA_CLOUD_PROVIDER);
    std::string get_user_nickname(const std::string& provider = ORCA_CLOUD_PROVIDER);
    std::string build_login_cmd(const std::string& provider = ORCA_CLOUD_PROVIDER);
    std::string build_logout_cmd(const std::string& provider = ORCA_CLOUD_PROVIDER);
    std::string build_login_info(const std::string& provider = ORCA_CLOUD_PROVIDER);
    std::string get_cloud_service_host(const std::string& provider = ORCA_CLOUD_PROVIDER);
    std::string get_cloud_login_url(const std::string& language = "", const std::string& provider = ORCA_CLOUD_PROVIDER);

    void enable_multi_machine(bool enable, const std::string& provider = ORCA_CLOUD_PROVIDER);

    // Profile synchronization methods
    // NOTE: this should always call only OrcaCloud
    int get_user_presets(std::map<std::string, std::map<std::string, std::string>>* user_presets, const std::string& provider = ORCA_CLOUD_PROVIDER);
    std::string request_setting_id(std::string name, std::map<std::string, std::string>* values_map, unsigned int* http_code, const std::string& provider = ORCA_CLOUD_PROVIDER);
    int put_setting(std::string setting_id, std::string name, std::map<std::string, std::string>* values_map, unsigned int* http_code, const std::string& provider = ORCA_CLOUD_PROVIDER, bool force = false);
    int get_setting_list2(std::string bundle_version, CheckFn chk_fn, ProgressFn pro_fn = nullptr, WasCancelledFn cancel_fn = nullptr, const std::string& provider = ORCA_CLOUD_PROVIDER);
    int delete_setting(std::string setting_id, const std::string& provider = ORCA_CLOUD_PROVIDER);

    int get_user_tasks(TaskQueryParams params, std::string* http_body, const std::string& provider = ORCA_CLOUD_PROVIDER);
    int get_printer_firmware(std::string dev_id, unsigned* http_code, std::string* http_body, const std::string& provider = ORCA_CLOUD_PROVIDER);
    int get_camera_url(std::string dev_id, std::function<void(std::string)> callback, const std::string& provider = ORCA_CLOUD_PROVIDER);
    int get_subtask(BBLModelTask* task, OnGetSubTaskFn getsub_fn, const std::string& provider = ORCA_CLOUD_PROVIDER);
    int get_my_profile(std::string token, unsigned int* http_code, std::string* http_body, const std::string& provider = ORCA_CLOUD_PROVIDER);
    int get_my_token(std::string ticket, unsigned int* http_code, std::string* http_body, const std::string& provider = ORCA_CLOUD_PROVIDER);

    // Printer agent methods
    int set_on_ssdp_msg_fn(OnMsgArrivedFn fn);
    int set_on_printer_connected_fn(OnPrinterConnectedFn fn);
    int set_on_subscribe_failure_fn(GetSubscribeFailureFn fn);
    int set_on_message_fn(OnMessageFn fn);
    int set_on_local_connect_fn(OnLocalConnectedFn fn);
    int set_on_local_message_fn(OnMessageFn fn);
    int set_server_callback(OnServerErrFn fn);
    int send_message(std::string dev_id, std::string json_str, int qos, int flag);
    int connect_printer(std::string dev_id, std::string dev_ip, std::string username, std::string password, bool use_ssl);
    int disconnect_printer();
    int send_message_to_printer(std::string dev_id, std::string json_str, int qos, int flag);
    void install_device_cert(std::string dev_id, bool lan_only);
    bool start_discovery(bool start, bool sending);
    int bind_detect(std::string dev_ip, std::string sec_link, detectResult& detect);
    int unbind(std::string dev_id);
    std::string get_user_selected_machine();
    int set_user_selected_machine(std::string dev_id);
    int start_subscribe(std::string module);
    int stop_subscribe(std::string module);
    int add_subscribe(std::vector<std::string> dev_list);
    int del_subscribe(std::vector<std::string> dev_list);
    int start_print(PrintParams params, OnUpdateStatusFn update_fn, WasCancelledFn cancel_fn, OnWaitFn wait_fn);
    int start_local_print_with_record(PrintParams params, OnUpdateStatusFn update_fn, WasCancelledFn cancel_fn, OnWaitFn wait_fn);
    int start_send_gcode_to_sdcard(PrintParams params, OnUpdateStatusFn update_fn, WasCancelledFn cancel_fn, OnWaitFn wait_fn);
    int start_local_print(PrintParams params, OnUpdateStatusFn update_fn, WasCancelledFn cancel_fn);
    int start_sdcard_print(PrintParams params, OnUpdateStatusFn update_fn, WasCancelledFn cancel_fn);
    FilamentSyncMode get_filament_sync_mode() const;
    bool fetch_filament_info(std::string dev_id);
    std::string to_orca_filament_id(const std::string& printer_filament_id) const;
    std::string from_orca_filament_id(const std::string& orca_filament_id) const;
    int request_bind_ticket(std::string* ticket);

private:
    struct PrinterCallbacks {
        OnMsgArrivedFn on_ssdp_msg_fn = nullptr;
        OnPrinterConnectedFn on_printer_connected_fn = nullptr;
        GetSubscribeFailureFn on_subscribe_failure_fn = nullptr;
        OnMessageFn on_message_fn = nullptr;
        OnLocalConnectedFn on_local_connect_fn = nullptr;
        OnMessageFn on_local_message_fn = nullptr;
        QueueOnMainFn queue_on_main_fn = nullptr;
        OnServerErrFn on_server_err_fn = nullptr;
    };

    void apply_printer_callbacks(const std::shared_ptr<IPrinterAgent>& printer_agent,
                                 const PrinterCallbacks& callbacks);
    PrinterCallbacks m_printer_callbacks;

    // Sub-agent composition
    // We support dynamic switching of printer agents (e.g. for different printer types), but the cloud agent is fixed at construction since
    // it's tied to the user's cloud account OrcaCloudServiceAgent is designed to be the primary cloud agent, but we support the possibility
    // of adding third-party cloud agents and delegating calls to them as needed
    std::map<std::string, std::shared_ptr<ICloudServiceAgent>> m_cloud_agents;
    std::shared_ptr<IPrinterAgent> m_printer_agent;
    std::string m_printer_agent_id;
};

}

#endif
