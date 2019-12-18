struct _zt_config {
    char *plugin_base;              /* Base directory for plugins */
    char *profile_file;             /* Path to profile file */
    int  thread_cap;                /* maximum threads to launch */
} typedef struct _zt_config zt_config;

zt_config  _g_config_data = {
    "/opt/ztsniff/plugins/",
    "zt_default.prof",
    128 };
GMutex     zt_config_mutex;
