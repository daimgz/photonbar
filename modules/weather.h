#ifndef WEATHER_H
#define WEATHER_H
#include <iostream>
using namespace std;
#include <cstring>
#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <array>
#include <curl/curl.h>
#include <json-c/json.h>
#include "module.h"
#include <fmt/format.h>
#include "../barElement.h"

class WeatherModule : public Module {
  private:
    // Elementos de la UI
    BarElement baseElement;

    // Configuración de ubicación
    const double lat;
    const double lon;

    // Estado del clima
    time_t lastApiCall;
    double temperature;
    double feelsLike;
    int humidity;
    double windSpeed;
    int weatherCode;
    bool isNight;
    bool showDetails;

    // Optimización: Conexión persistente y cache
    CURL* curl_handle;
    time_t lastModified;

    // Descripciones del clima (códigos WMO)
    std::array<const char*, 10> weather_descriptions = {
      "Despejado", "Parcialmente nublado", "Nublado", "Lluvia ligera",
      "Lluvia moderada", "Lluvia fuerte", "Nieve", "Niebla",
      "Tormenta", "Variable"
    };

    // Callbacks para curl
    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp) {
      userp->append((char*)contents, size * nmemb);
      return size * nmemb;
    }

    static size_t HeaderCallback(void *contents, size_t size, size_t nmemb, void *userp) {
      size_t total_size = size * nmemb;
      std::string header((char*)contents, total_size);

      // Buscar header Last-Modified para cache
      if (header.find("Last-Modified:") == 0) {
        // Extraer timestamp del header
        WeatherModule* self = static_cast<WeatherModule*>(userp);
        // Parse simple - would need proper date parsing in production
        self->lastModified = time(nullptr);
      }

      return total_size;
    }

  public:
    WeatherModule():
      Module("weather", false, 600),  // No auto-update, cada 600 segundos (10 min)
      lat(-34.4476799),    // Del Viso, Pilar, Buenos Aires
      lon(-58.8052387),    // Del Viso, Pilar, Buenos Aires
      lastApiCall(0),
      temperature(0.0),
      feelsLike(0.0),
      humidity(0),
      windSpeed(0.0),
      weatherCode(0),
      isNight(false),
      showDetails(false),
      curl_handle(nullptr),
      lastModified(0)
    {
      // Inicializar curl una sola vez para conexión persistente
      curl_global_init(CURL_GLOBAL_DEFAULT);
      initializeCurlHandle();

      // Configurar elemento base
      baseElement.moduleName = name;

      // Click izquierdo: toggle detalles
      baseElement.setEvent(BarElement::CLICK_LEFT, [this]() {
        showDetails = !showDetails;
        update();
        if (renderFunction) {
          renderFunction();
        }
      });

      // Color base del texto
      baseElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));

      elements.push_back(&baseElement);
    }

    void initializeCurlHandle() {
      if (!curl_handle) {
        curl_handle = curl_easy_init();
        if (!curl_handle) {
          fprintf(stderr, "[WeatherModule] Failed to initialize curl handle\n");
          return;
        }

        // Optimización 1: Keep-Alive y TCP persistente
        curl_easy_setopt(curl_handle, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curl_handle, CURLOPT_TCP_KEEPINTVL, 30L);

        // Optimización 2: Reutilización de sesión SSL
        curl_easy_setopt(curl_handle, CURLOPT_SSL_SESSIONID_CACHE, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);

        // Optimización 3: HTTP cache con If-Modified-Since
        curl_easy_setopt(curl_handle, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);

        // Configuración general
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, this);

        fprintf(stderr, "[WeatherModule] Curl handle initialized with optimizations\n");
      }
    }

    ~WeatherModule() {
      if (curl_handle) {
        curl_easy_cleanup(curl_handle);
      }
      curl_global_cleanup();
    }

    bool initialize() override {
      // Realizar primera llamada a la API
      return fetchWeatherData();
    }

    void update() override {
      time_t now = time(nullptr);

      // Asegurar que el handle esté inicializado
      if (!curl_handle) {
        initializeCurlHandle();
      }

      // Solo llamar a la API si han pasado 10 minutos
      if (fetchWeatherData()) {
        lastApiCall = now;
      }

      generateBuffer();
      lastUpdate = time(nullptr);
    }

  private:
    bool fetchWeatherData() {
      if (!curl_handle) {
        fprintf(stderr, "[WeatherModule] Curl handle not initialized\n");
        return false;
      }

      std::string readBuffer;

      // Construir URL para Open-Meteo API
      std::string url = fmt::format(
        "https://api.open-meteo.com/v1/forecast?"
        "latitude={:.6f}&longitude={:.6f}&"
        "current_weather=true&"
        "hourly=temperature_2m,apparent_temperature,relativehumidity_2m,windspeed_10m&"
        "timezone=America/Argentina/Buenos_Aires",
        lat, lon
      );

      // Optimización 4: Usar timestamp de última modificación
      curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &readBuffer);
      curl_easy_setopt(curl_handle, CURLOPT_TIMEVALUE, lastModified);

      CURLcode res = curl_easy_perform(curl_handle);

      bool success = false;
      if(res == CURLE_OK) {
        long http_code = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 200) {
          // Datos nuevos recibidos
          success = parseWeatherJson(readBuffer);
          lastModified = time(nullptr); // Actualizar timestamp
          fprintf(stderr, "[WeatherModule] Fresh data received (HTTP 200)\n");
        } else if (http_code == 304) {
          // Not Modified - usar cache existente
          success = true;
          fprintf(stderr, "[WeatherModule] Using cached data (HTTP 304)\n");
        } else {
          fprintf(stderr, "[WeatherModule] HTTP error: %ld\n", http_code);
        }
      } else {
        fprintf(stderr, "[WeatherModule] Curl error: %s\n", curl_easy_strerror(res));
      }

      return success;
    }

    bool parseWeatherJson(const std::string& json_str) {
      json_object *root = json_tokener_parse(json_str.c_str());
      if (!root) {
        fprintf(stderr, "[WeatherModule] Failed to parse JSON\n");
        return false;
      }

      json_object *current_weather;
      if (!json_object_object_get_ex(root, "current_weather", &current_weather)) {
        fprintf(stderr, "[WeatherModule] No current_weather in JSON\n");
        json_object_put(root);
        return false;
      }

      // Extraer temperatura
      json_object *temp_obj;
      if (json_object_object_get_ex(current_weather, "temperature", &temp_obj)) {
        temperature = json_object_get_double(temp_obj);
      }

      // Extraer código del clima
      json_object *weathercode_obj;
      if (json_object_object_get_ex(current_weather, "weathercode", &weathercode_obj)) {
        weatherCode = json_object_get_int(weathercode_obj);
      }

      // Extraer si es de noche
      json_object *is_day_obj;
      if (json_object_object_get_ex(current_weather, "is_day", &is_day_obj)) {
        isNight = !json_object_get_boolean(is_day_obj);
      }

      // Extraer datos horarios para feels like, humedad y viento
      json_object *hourly;
      if (json_object_object_get_ex(root, "hourly", &hourly)) {
        json_object *temp_2m_array;
        json_object *feels_array;
        json_object *humidity_array;
        json_object *wind_array;

        if (json_object_object_get_ex(hourly, "temperature_2m", &temp_2m_array) &&
            json_object_object_get_ex(hourly, "apparent_temperature", &feels_array) &&
            json_object_object_get_ex(hourly, "relativehumidity_2m", &humidity_array) &&
            json_object_object_get_ex(hourly, "windspeed_10m", &wind_array)) {

          // Usar el primer valor (hora actual)
          if (json_object_array_length(temp_2m_array) > 0) {
            feelsLike = json_object_get_double(json_object_array_get_idx(feels_array, 0));
            humidity = json_object_get_int(json_object_array_get_idx(humidity_array, 0));
            windSpeed = json_object_get_double(json_object_array_get_idx(wind_array, 0));
          }
        }
      }

      json_object_put(root);

      fprintf(stderr, "[WeatherModule] Weather updated: %.1f°C, code: %d\n",
              temperature, weatherCode);

      return true;
    }

    const char* getWeatherDescription() {
      // Simplificación de códigos WMO a descripciones
      switch(weatherCode) {
        case 0: return isNight ? u8"\U000f0594" : u8"\U000f0599";  // Despejado
        case 1:
        case 2:
        case 3: return isNight ? u8"\U000f0f31" : u8"\U000f0595";  // Parcialmente nublado
        case 45:
        case 48: return u8"\U000f0591";  // Niebla
        case 51:
        case 53:
        case 55: return u8"\U000f0f33";  // Lluvia ligera
        case 56:
        case 57: return u8"\U000f0597";  // Lluvia helada ligera
        case 61:
        case 63:
        case 65:
        case 66:
        case 67: return u8"\U000f0596";  // 󰖖 Lluvia helada fuerte
        case 71:
        case 73:
        case 75: return u8"\U000f0f36";  // Nieve
        case 77: return u8"\U000f0592";  // Granizo
        case 80:
        case 81:
        case 82: return u8"\U000f067e";  // 󰖓 Tormentas
        case 85:
        case 86: return u8"\U000f0598";  // Nevadas ligeras
        default: return "\uf2c7";  // Desconocido
      }
    }

    void generateBuffer() {
      std::string content;

      // Icono del clima y temperatura principal
      content += fmt::format("{} {:.1f}°C", getWeatherDescription(), temperature);

      if (showDetails) {
        content += fmt::format(
          " | ST: {:.1f}°C | H: {}% | V: {:.1f}km/h",
          feelsLike, humidity, windSpeed
        );
      }

      // Copiar al buffer del elemento
      baseElement.contentLen = snprintf(
        baseElement.content,
        CONTENT_MAX_LEN,
        "%s",
        content.c_str()
      );

      baseElement.dirtyContent = true;

      // Cambiar color según estado
      if (temperature > 30) {
        // Muy caliente - rojo
        baseElement.foregroundColor = Color::parse_color("#FF6B6B", NULL, Color(255, 107, 107, 255));
      } else if (temperature < 5) {
        // Muy frío - azul
        baseElement.foregroundColor = Color::parse_color("#6BB6FF", NULL, Color(107, 182, 255, 255));
      } else {
        // Normal - color por defecto
        baseElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
      }
    }
};

#endif // WEATHER_H
