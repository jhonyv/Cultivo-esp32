#pragma once

// Bot de Telegram para ver y manejar el equipo desde cualquier lado, y recibir alertas.
// Corre en su propia tarea (las conexiones HTTPS bloquean); los cambios a los relés
// se encolan y se aplican desde el loop principal con telegramLoop().
void telegramBegin();
void telegramLoop();
