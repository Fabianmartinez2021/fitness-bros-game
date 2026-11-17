#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#endif

#include "httplib.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Usuario {
    std::string nombre;
    char sexo = 'M';
    int edad = 0;
    double peso = 0.0;
    double altura_cm = 0.0;
    double actividad = 1.55;
};

struct PlanNutricional {
    double imc = 0.0;
    std::string clasificacion_imc;
    std::string objetivo;
    double tmb = 0.0;
    double tdee = 0.0;
    double calorias_objetivo = 0.0;
    double proteina_g = 0.0;
    double grasa_g = 0.0;
    double carbohidrato_g = 0.0;
};

struct Validacion {
    bool ok = true;
    std::string mensaje;
};

static std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

static std::string htmlEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += ch; break;
        }
    }
    return out;
}

static bool parseDouble(const std::string& raw, double& value) {
    if (raw.empty()) return false;
    std::istringstream iss(raw);
    iss >> value;
    return iss.eof() && !iss.fail();
}

static bool parseInt(const std::string& raw, int& value) {
    if (raw.empty()) return false;
    std::istringstream iss(raw);
    iss >> value;
    return iss.eof() && !iss.fail();
}

static Validacion validarUsuario(const Usuario& u) {
    if (u.nombre.empty() || u.nombre.size() < 2) return {false, "El nombre debe tener al menos 2 caracteres."};
    if (u.sexo != 'M' && u.sexo != 'F') return {false, "Selecciona un sexo válido."};
    if (u.edad <= 12 || u.edad >= 100) return {false, "La edad debe estar entre 13 y 99 años."};
    if (u.peso <= 20.0 || u.peso >= 350.0) return {false, "El peso debe estar entre 20 y 350 kg."};
    if (u.altura_cm <= 100.0 || u.altura_cm >= 250.0) return {false, "La altura debe estar entre 100 y 250 cm."};
    return {true, ""};
}

static PlanNutricional calcularPlan(const Usuario& u) {
    PlanNutricional plan;
    const double altura_m = u.altura_cm / 100.0;
    plan.imc = u.peso / (altura_m * altura_m);

    if (plan.imc < 18.5) {
        plan.clasificacion_imc = "Bajo peso";
        plan.objetivo = "Superávit calórico (Ganancia muscular)";
    } else if (plan.imc < 25.0) {
        plan.clasificacion_imc = "Peso ideal";
        plan.objetivo = "Mantenimiento y recomposición";
    } else {
        plan.clasificacion_imc = "Sobrepeso / Obesidad";
        plan.objetivo = "Déficit calórico (Pérdida de grasa)";
    }

    if (u.sexo == 'M') {
        plan.tmb = (10.0 * u.peso) + (6.25 * u.altura_cm) - (5.0 * u.edad) + 5.0;
    } else {
        plan.tmb = (10.0 * u.peso) + (6.25 * u.altura_cm) - (5.0 * u.edad) - 161.0;
    }

    plan.tdee = plan.tmb * u.actividad;
    
    if (plan.imc < 18.5) plan.calorias_objetivo = plan.tdee + 300.0;
    else if (plan.imc >= 25.0) plan.calorias_objetivo = plan.tdee - 400.0;
    else plan.calorias_objetivo = plan.tdee;

    if (plan.calorias_objetivo < 1200.0) plan.calorias_objetivo = 1200.0;

    plan.proteina_g = u.peso * 2.0;
    plan.grasa_g = u.peso * 0.8;
    double kcal_pro = plan.proteina_g * 4.0;
    double kcal_gra = plan.grasa_g * 9.0;
    double kcal_car = plan.calorias_objetivo - kcal_pro - kcal_gra;
    if (kcal_car < 0) kcal_car = 0;
    plan.carbohidrato_g = kcal_car / 4.0;

    return plan;
}

static std::string generarPaginaJuego(const Usuario& u, const PlanNutricional& plan) {
    int calInt = (int)std::round(plan.calorias_objetivo);
    int protInt = (int)std::round(plan.proteina_g);
    int carbInt = (int)std::round(plan.carbohidrato_g);
    int grasInt = (int)std::round(plan.grasa_g);

    char imcStr[16];
    snprintf(imcStr, sizeof(imcStr), "%.1f", plan.imc);

    std::string avatarImg = (u.sexo == 'M') ? "/mario-head.png" : "/peach-head.png";

    return R"HTML(<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Fitness Bros Game - Desafío</title>
    <link rel="stylesheet" href="/styles.css">
    <style>
        .nintendo-card { max-width: 680px !important; position: relative; }
        .hud-bars { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 12px; text-align: left; }
        .hud-box { background: #f8fafc; border: 1px solid #cbd5e1; padding: 8px 12px; border-radius: 12px; }
        .hud-label { font-size: 11px; font-weight: 700; display: flex; justify-content: space-between; margin-bottom: 4px; }
        .progress-track { width: 100%; height: 10px; background: #e2e8f0; border-radius: 6px; overflow: hidden; }
        .progress-fill-prot { height: 100%; width: 0%; background: linear-gradient(90deg, #16a34a, #22c55e); transition: width 0.15s; }
        .progress-fill-cal { height: 100%; width: 0%; background: linear-gradient(90deg, #f59e0b, #dc2626); transition: width 0.15s; }
        #gameCanvas { background: linear-gradient(180deg, #38bdf8 0%, #bae6fd 82%, #15803d 82%, #166534 100%); border-radius: 14px; border: 2px solid #cbd5e1; display: block; margin: 0 auto; cursor: none; width: 100%; max-width: 580px; height: 260px; }
        .game-controls-hint { font-size: 11px; color: #64748b; margin-top: 6px; text-align: center; }
        .result-banner { padding: 14px; border-radius: 14px; margin-bottom: 16px; text-align: center; }
        .banner-win { background: #dcfce7; border: 2px solid #16a34a; color: #14532d; }
        .banner-lose { background: #fee2e2; border: 2px solid #dc2626; color: #7f1d1d; }
        .routine-container { background: #f8fafc; border: 1px solid #cbd5e1; border-radius: 14px; padding: 16px; margin-top: 16px; text-align: left; }
        .routine-day { margin-bottom: 12px; padding-bottom: 8px; border-bottom: 1px dashed #cbd5e1; font-size: 13px; }
        .routine-day:last-child { border-bottom: none; margin-bottom: 0; padding-bottom: 0; }
        .music-btn { position: absolute; top: 16px; right: 16px; background: #f8fafc; border: 2px solid #cbd5e1; border-radius: 50%; width: 36px; height: 36px; cursor: pointer; display: flex; align-items: center; justify-content: center; font-size: 15px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); z-index: 10; transition: transform 0.2s; }
        .music-btn:hover { transform: scale(1.08); background: #f1f5f9; }
    </style>
</head>
<body>
    <audio id="bgMusic" loop><source src="/musica.mp3" type="audio/mpeg"></audio>
    <audio id="eatSound"><source src="/comer.mp3" type="audio/mpeg"></audio>
    <audio id="winSound"><source src="/winner.mp3" type="audio/mpeg"></audio>
    <audio id="loseSound"><source src="/game_over.mp3" type="audio/mpeg"></audio>

    <div class="nintendo-bg">
        <div class="floor-dumbbell dumbbell-1 animate-float-slow"><img src="/mancuerna.png" alt="Mancuerna"></div>
        <div class="floor-dumbbell dumbbell-2 animate-float-delayed"><img src="/mancuerna.png" alt="Mancuerna"></div>
        <div class="scene-container">
            
            <div class="nintendo-card">
                <button type="button" class="music-btn" id="musicToggleBtn" onclick="toggleMusic()" title="Silenciar / Reproducir música">🔇</button>

                <!-- PANTALLA 1: EL MINIJUEGO -->
                <div id="game-screen">
                    <div class="card-header" style="margin-bottom: 12px; padding-bottom: 10px;">
                        <img src="/titulo.png" alt="Fitness Bros Game" class="app-title-img-small">
                        <span id="timer-badge" class="step-counter">35s</span>
                    </div>

                    <div class="hud-bars">
                        <div class="hud-box">
                            <div class="hud-label">
                                <span>Proteína (Meta)</span>
                                <span id="prot-text">0g / )HTML" + std::to_string(protInt) + R"HTML(g</span>
                            </div>
                            <div class="progress-track">
                                <div id="prot-bar" class="progress-fill-prot"></div>
                            </div>
                        </div>
                        <div class="hud-box">
                            <div class="hud-label">
                                <span>Calorías (Límite)</span>
                                <span id="cal-text">0 / )HTML" + std::to_string(calInt) + R"HTML( kcal</span>
                            </div>
                            <div class="progress-track">
                                <div id="cal-bar" class="progress-fill-cal"></div>
                            </div>
                        </div>
                    </div>

                    <canvas id="gameCanvas" width="580" height="260"></canvas>
                    <p class="game-controls-hint">Usa el mouse o las flechas para atrapar proteínas y esquivar excesos</p>
                </div>

                <!-- PANTALLA 2: RESUMEN Y RUTINA DE ALTO IMPACTO -->
                <div id="results-screen" style="display: none;">
                    
                    <div class="impact-banner">
                        <div id="game-outcome-banner" style="margin-bottom: 10px;"></div>
                        <div class="impact-diagnostic">
                            )HTML" + htmlEscape(u.nombre) + R"HTML( TIENES: )HTML" + htmlEscape(plan.clasificacion_imc) + R"HTML(!
                        </div>
                        <p style="font-size: 12px; color: #cbd5e1; margin-top: 4px;">Tu meta será: <b>)HTML" + htmlEscape(plan.objetivo) + R"HTML(</b></p>
                    </div>

                    <div class="impact-stats-box">
                        <div class="impact-stats-row">
                            <span>Tu IMC es de:</span>
                            <strong style="color: #0f172a;">)HTML" + std::string(imcStr) + R"HTML(</strong>
                        </div>
                        <div class="impact-stats-row">
                            <span>Tu Déficit Calórico es de:</span>
                            <strong style="color: #dc2626;">)HTML" + std::to_string(calInt) + R"HTML( kcal</strong>
                        </div>
                    </div>

                    <div class="impact-warning-text">
                        <b>Eso significa que debes cuidar tu alimentación y no pasarte de estas calorías.</b><br>
                        También debes consumir al día:
                    </div>

                    <h3 style="font-size:12px; color:#0f172a; margin-bottom:6px; text-align:center;">DISTRIBUCIÓN DE MACRONUTRIENTES DIARIOS:</h3>
                    <div class="macros-grid">
                        <div class="macro-card" style="border-color:#16a34a;">
                            <h4>Proteínas</h4>
                            <p style="color:#16a34a;">)HTML" + std::to_string(protInt) + R"HTML(g</p>
                        </div>
                        <div class="macro-card" style="border-color:#2563eb;">
                            <h4>Carbos</h4>
                            <p style="color:#2563eb;">)HTML" + std::to_string(carbInt) + R"HTML(g</p>
                        </div>
                        <div class="macro-card" style="border-color:#d97706;">
                            <h4>Grasas</h4>
                            <p style="color:#d97706;">)HTML" + std::to_string(grasInt) + R"HTML(g</p>
                        </div>
                    </div>

                    <div style="margin-top: 16px;" id="routine-trigger-container">
                        <button type="button" class="btn-solid" onclick="mostrarSelectorRutina()" style="width: 100%; font-size: 15px; padding: 14px;">¿Armo tu rutina de GYM?</button>
                    </div>

                    <div id="routine-selector" style="display: none; margin-top: 16px; background: #f8fafc; border: 1px solid #cbd5e1; border-radius: 14px; padding: 14px; text-align: left;">
                        <label style="font-size: 13px; font-weight: bold; color: #0f172a; display: block; margin-bottom: 8px;">¿Cuántos días por semana quieres entrenar?</label>
                        <div style="display: flex; gap: 8px; margin-bottom: 12px;">
                            <button type="button" class="btn-outline" onclick="generarRutinaGym(3)" style="flex:1;">3 Días</button>
                            <button type="button" class="btn-outline" onclick="generarRutinaGym(5)" style="flex:1;">5 Días</button>
                            <button type="button" class="btn-outline" onclick="generarRutinaGym(7)" style="flex:1;">7 Días</button>
                        </div>
                    </div>

                    <div id="routine-result-box" style="display: none;"></div>

                    <div class="wizard-buttons" style="margin-top: 16px;">
                        <button type="button" class="btn-outline" onclick="location.reload()">Jugar de Nuevo</button>
                        <a href="/" class="btn-solid" style="text-decoration:none; display:flex; align-items:center; justify-content:center;">Ir al Inicio</a>
                    </div>
                </div>

            </div>

            <div class="mario-overlay-side">
                <img src="/mario.png" alt="Mario" class="mario-img">
            </div>

        </div>
    </div>

    <script>
        const music = document.getElementById('bgMusic');
        const eatSound = document.getElementById('eatSound');
        const winSound = document.getElementById('winSound');
        const loseSound = document.getElementById('loseSound');
        const musicBtn = document.getElementById('musicToggleBtn');
        let musicPlaying = false;

        function toggleMusic() {
            if (musicPlaying) {
                music.pause();
                musicBtn.innerText = "🔇";
                musicPlaying = false;
            } else {
                music.play().then(() => {
                    musicBtn.innerText = "🎵";
                    musicPlaying = true;
                }).catch(e => {});
            }
        }

        function playEatSound() {
            eatSound.currentTime = 0;
            eatSound.play().catch(e => {});
        }

        window.addEventListener('load', () => {
            music.volume = 0.5;
            music.play().then(() => {
                musicBtn.innerText = "🎵";
                musicPlaying = true;
            }).catch(error => {
                const activarAudio = () => {
                    if (!musicPlaying) toggleMusic();
                    window.removeEventListener('click', activarAudio);
                    window.removeEventListener('keydown', activarAudio);
                };
                window.addEventListener('click', activarAudio);
                window.addEventListener('keydown', activarAudio);
            });
        });

        const META_PROTEINA = )HTML" + std::to_string(protInt) + R"HTML(;
        const LIMITE_CALORIAS = )HTML" + std::to_string(calInt) + R"HTML(;
        const AVATAR_SRC = ")HTML" + avatarImg + R"HTML(";
        const USER_PESO = )HTML" + std::to_string(u.peso) + R"HTML(;

        const canvas = document.getElementById('gameCanvas');
        const ctx = canvas.getContext('2d');

        const playerImg = new Image();
        playerImg.src = AVATAR_SRC;

        let currentProt = 0;
        let currentCal = 0;
        let timeLeft = 35;
        let gameOver = false;

        const player = {
            x: canvas.width / 2 - 25,
            y: canvas.height - 55,
            width: 50,
            height: 50,
            speed: 7
        };

        const foods = [
            { emoji: "🍗", prot: Math.ceil(META_PROTEINA * 0.20), cal: Math.ceil(LIMITE_CALORIAS * 0.08), speed: 1.2 },
            { emoji: "🥚", prot: Math.ceil(META_PROTEINA * 0.12), cal: Math.ceil(LIMITE_CALORIAS * 0.05), speed: 1.3 },
            { emoji: "🥤", prot: Math.ceil(META_PROTEINA * 0.25), cal: Math.ceil(LIMITE_CALORIAS * 0.06), speed: 1.5 },
            { emoji: "🥦", prot: Math.ceil(META_PROTEINA * 0.04), cal: Math.ceil(LIMITE_CALORIAS * 0.02), speed: 1.0 },
            { emoji: "🍕", prot: 2, cal: Math.ceil(LIMITE_CALORIAS * 0.25), speed: 1.3 },
            { emoji: "🍔", prot: 3, cal: Math.ceil(LIMITE_CALORIAS * 0.30), speed: 1.4 },
            { emoji: "🍩", prot: 0, cal: Math.ceil(LIMITE_CALORIAS * 0.20), speed: 1.5 }
        ];

        let fallingItems = [];
        let keys = {};

        window.addEventListener('keydown', e => keys[e.key] = true);
        window.addEventListener('keyup', e => keys[e.key] = false);
        canvas.addEventListener('mousemove', e => {
            const rect = canvas.getBoundingClientRect();
            const scaleX = canvas.width / rect.width;
            player.x = (e.clientX - rect.left) * scaleX - player.width / 2;
        });

        const spawnInterval = setInterval(() => {
            if (gameOver) return;
            const template = foods[Math.floor(Math.random() * foods.length)];
            fallingItems.push({
                ...template,
                x: Math.random() * (canvas.width - 40) + 10,
                y: -30,
                size: 28
            });
        }, 800);

        const timerInterval = setInterval(() => {
            if (gameOver) return;
            timeLeft--;
            document.getElementById('timer-badge').innerText = `${timeLeft}s`;
            if (timeLeft <= 0) {
                endGame(false, "Se acabó el tiempo. No lograste completar tu meta de proteína diaria.");
            }
        }, 1000);

        function updateHUD() {
            document.getElementById('prot-text').innerText = `${currentProt}g / ${META_PROTEINA}g`;
            document.getElementById('cal-text').innerText = `${currentCal} / ${LIMITE_CALORIAS} kcal`;

            const protPct = Math.min(100, (currentProt / META_PROTEINA) * 100);
            const calPct = Math.min(100, (currentCal / LIMITE_CALORIAS) * 100);

            document.getElementById('prot-bar').style.width = protPct + "%";
            document.getElementById('cal-bar').style.width = calPct + "%";
        }

        function endGame(won, reason) {
            gameOver = true;
            clearInterval(spawnInterval);
            clearInterval(timerInterval);

            music.pause();
            musicPlaying = false;
            musicBtn.innerText = "🔇";

            if (won) {
                winSound.currentTime = 0;
                winSound.play().catch(e => {});
            } else {
                loseSound.currentTime = 0;
                loseSound.play().catch(e => {});
            }

            document.getElementById('game-screen').style.display = 'none';
            const resultsDiv = document.getElementById('results-screen');
            const banner = document.getElementById('game-outcome-banner');

            banner.className = 'result-banner ' + (won ? 'banner-win' : 'banner-lose');
            banner.innerHTML = `
                <h3 style="font-size:18px; font-weight:900; margin-bottom:4px; text-transform:uppercase;">${won ? '¡RETO SUPERADO!' : '¡TE PASASTE DE CALORÍAS!'}</h3>
                <p style="font-size:13px;">${reason}</p>
            `;

            resultsDiv.style.display = 'block';
        }

        function gameLoop() {
            if (gameOver) return;

            ctx.clearRect(0, 0, canvas.width, canvas.height);

            if (keys['ArrowLeft'] || keys['a']) player.x -= player.speed;
            if (keys['ArrowRight'] || keys['d']) player.x += player.speed;

            if (player.x < 0) player.x = 0;
            if (player.x + player.width > canvas.width) player.x = canvas.width - player.width;

            const fatScale = 1 + (currentCal / LIMITE_CALORIAS) * 0.2;
            const drawW = player.width * fatScale;
            const drawH = player.height;

            if (playerImg.complete && playerImg.naturalWidth !== 0) {
                ctx.drawImage(playerImg, player.x - (drawW - player.width)/2, player.y, drawW, drawH);
            } else {
                ctx.fillStyle = "#dc2626";
                ctx.fillRect(player.x, player.y, drawW, drawH);
            }

            ctx.font = "26px Arial";
            for (let i = fallingItems.length - 1; i >= 0; i--) {
                let item = fallingItems[i];
                item.y += item.speed;
                ctx.fillText(item.emoji, item.x, item.y);

                if (
                    item.x < player.x + drawW &&
                    item.x + item.size > player.x &&
                    item.y > player.y &&
                    item.y - item.size < player.y + drawH
                ) {
                    playEatSound();

                    currentProt += item.prot;
                    currentCal += item.cal;
                    updateHUD();
                    fallingItems.splice(i, 1);

                    if (currentCal > LIMITE_CALORIAS) {
                        endGame(false, `Superaste tu límite diario de ${LIMITE_CALORIAS} kcal con comida chatarra.`);
                        return;
                    }
                    if (currentProt >= META_PROTEINA) {
                        endGame(true, `Excelente. Completaste tus ${META_PROTEINA}g de proteína dentro del margen calórico.`);
                        return;
                    }
                    continue;
                }

                if (item.y > canvas.height + 30) {
                    fallingItems.splice(i, 1);
                }
            }

            requestAnimationFrame(gameLoop);
        }

        requestAnimationFrame(gameLoop);

        function mostrarSelectorRutina() {
            document.getElementById('routine-trigger-container').style.display = 'none';
            document.getElementById('routine-selector').style.display = 'block';
        }

        function generarRutinaGym(dias) {
            const box = document.getElementById('routine-result-box');
            box.style.display = 'block';
            
            let htmlContent = `<div class="routine-container">
                <h3 style="font-size:15px; color:#b91c1c; margin-bottom:10px;">Tu Rutina Personalizada (${dias} Días por Semana)</h3>
                <p style="font-size:12px; color:#64748b; margin-bottom:12px;">Adaptada para tu peso actual de <b>${USER_PESO} kg</b> enfocada en rendimiento óptimo.</p>`;

            if (dias === 3) {
                htmlContent += `
                    <div class="routine-day"><strong>Día 1 · Full Body A:</strong> Sentadilla con barra 4x8, Press de banca 4x8, Remo con barra 4x10.</div>
                    <div class="routine-day"><strong>Día 2 · Full Body B:</strong> Peso Muerto 3x6, Jalón al pecho 4x10, Press Militar 3x10.</div>
                    <div class="routine-day"><strong>Día 3 · Full Body C:</strong> Prensa 4x10, Fondos en paralelas 3x10, Curl Bíceps + Extensiones 3x12.</div>`;
            } else if (dias === 5) {
                htmlContent += `
                    <div class="routine-day"><strong>Día 1 · Push:</strong> Press banca, Press inclinado, Elevaciones laterales, Tríceps.</div>
                    <div class="routine-day"><strong>Día 2 · Pull:</strong> Dominadas, Remo con barra, Face pulls, Bíceps.</div>
                    <div class="routine-day"><strong>Día 3 · Legs:</strong> Sentadilla, Prensa, Extensiones, Gemelos.</div>
                    <div class="routine-day"><strong>Día 4 · Upper:</strong> Press militar, Jalón polea, Aperturas, Brazos.</div>
                    <div class="routine-day"><strong>Día 5 · Lower:</strong> Peso muerto rumano, Hip Thrust, Curl femoral, Abdominales.</div>`;
            } else {
                htmlContent += `
                    <div class="routine-day"><strong>Día 1 · Lunes:</strong> Pecho & Tríceps (Hipertrofia).</div>
                    <div class="routine-day"><strong>Día 2 · Martes:</strong> Espalda & Bíceps (Densidad).</div>
                    <div class="routine-day"><strong>Día 3 · Miércoles:</strong> Piernas Completo.</div>
                    <div class="routine-day"><strong>Día 4 · Jueves:</strong> Hombros & Abdomen.</div>
                    <div class="routine-day"><strong>Día 5 · Viernes:</strong> Brazos & Pecho Liviano.</div>
                    <div class="routine-day"><strong>Día 6 · Sábado:</strong> Tren Inferior Detalle.</div>
                    <div class="routine-day"><strong>Día 7 · Domingo:</strong> Descanso Activo / Movilidad.</div>`;
            }

            htmlContent += `</div>`;
            box.innerHTML = htmlContent;
        }
    </script>
</body>
</html>)HTML";
}

int main() {
    httplib::Server svr;
    svr.set_mount_point("/", "./public");

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("./public/index.html");
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html; charset=utf-8");
        } else {
            res.set_content("Error: No se encuentra public/index.html", "text/plain");
        }
    });

    svr.Post("/calcular", [](const httplib::Request& req, httplib::Response& res) {
        Usuario u;
        u.nombre = trim(req.get_param_value("nombre"));
        
        std::string sexo_str = req.get_param_value("sexo");
        u.sexo = (sexo_str == "F" ? 'F' : 'M');

        parseInt(req.get_param_value("edad"), u.edad);
        parseDouble(req.get_param_value("peso"), u.peso);
        parseDouble(req.get_param_value("estatura"), u.altura_cm);
        parseDouble(req.get_param_value("actividad"), u.actividad);

        Validacion val = validarUsuario(u);
        if (!val.ok) {
            std::cout << "[ERROR DE VALIDACION C++]: " << val.mensaje << std::endl;
            res.set_redirect("/");
            return;
        }

        PlanNutricional plan = calcularPlan(u);
        std::string html = generarPaginaJuego(u, plan);
        res.set_content(html, "text/html; charset=utf-8");
    });

    std::cout << "Servidor Fitness Bros Game corriendo en http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}