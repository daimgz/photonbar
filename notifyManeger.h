#ifndef NOTIFY_MANAGER_H
#define NOTIFY_MANAGER_H

#include <libnotify/notify.h>
#include <string>

class NotifyManager {
public:
    // Inicializa libnotify al crear la instancia
    NotifyManager(const std::string& appName) {
        notify_init(appName.c_str());
    }

    // Limpia libnotify al destruir la instancia
    ~NotifyManager() {
        if (notify_is_initted()) {
            notify_uninit();
        }
    }

    // Método genérico para enviar notificaciones
    void send(const std::string& title, const std::string& body,
              NotifyUrgency urgency = NOTIFY_URGENCY_NORMAL,
              const std::string& icon = "") {

        NotifyNotification* n = notify_notification_new(
            title.c_str(),
            body.c_str(),
            icon.empty() ? nullptr : icon.c_str()
        );

        notify_notification_set_urgency(n, urgency);

        // Mostrar la notificación
        notify_notification_show(n, nullptr);

        // Liberar la memoria del objeto de notificación
        g_object_unref(G_OBJECT(n));
    }

    // Singleton para acceder fácilmente desde cualquier módulo
    static NotifyManager& instance() {
        static NotifyManager instance("PhotonBar");
        return instance;
    }

private:
    // Evitar copias
    NotifyManager(const NotifyManager&) = delete;
    void operator=(const NotifyManager&) = delete;
};

#endif
