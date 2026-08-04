#include <Preferences.h>
#include "app_config.h"
#include "config.h"

namespace {
String currentServerUrl;
String currentLastJobId;
}

namespace AppConfig {
void begin() {
    Preferences prefs;
    prefs.begin(Config::PREF_APP_NAMESPACE, true);
    currentServerUrl = prefs.getString(Config::PREF_KEY_SERVER_URL, Config::DEFAULT_SERVER_URL);
    currentLastJobId = prefs.getString(Config::PREF_KEY_LAST_JOB, "");
    prefs.end();
}

String serverUrl() { return currentServerUrl; }

void setServerUrl(const String& url) {
    currentServerUrl = url;
    Preferences prefs;
    prefs.begin(Config::PREF_APP_NAMESPACE, false);
    prefs.putString(Config::PREF_KEY_SERVER_URL, currentServerUrl);
    prefs.end();
}

String lastJobId() { return currentLastJobId; }

void setLastJobId(const String& jobId) {
    currentLastJobId = jobId;
    Preferences prefs;
    prefs.begin(Config::PREF_APP_NAMESPACE, false);
    prefs.putString(Config::PREF_KEY_LAST_JOB, currentLastJobId);
    prefs.end();
}
}
