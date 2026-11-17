# PulseLab — Guía de defensa del proyecto (código a código)

Documento para **entender, explicar y defender** PulseLab: evaluación física, calorías, macros, rutina de gimnasio y menú, con backend **solo C++** y frontend **HTML5 + CSS3 sin JavaScript**.

Si un jurado pregunta “¿dónde está esto en el código?”, este README indica el archivo, la función y el porqué.

---

## Índice

1. [Qué es el proyecto y qué problema resuelve](#1-qué-es-el-proyecto-y-qué-problema-resuelve)
2. [Arquitectura (cómo viaja un clic hasta el resultado)](#2-arquitectura-cómo-viaja-un-clic-hasta-el-resultado)
3. [Por qué C++ y por qué cero JavaScript](#3-por-qué-c-y-por-qué-cero-javascript)
4. [Estructura de carpetas](#4-estructura-de-carpetas)
5. [Librería `httplib.h`](#5-librería-httplibh)
6. [Compilar y ejecutar en local](#6-compilar-y-ejecutar-en-local)
7. [`public/index.html` campo por campo](#7-publicindexhtml-campo-por-campo)
8. [`public/styles.css` clase por clase](#8-publicstylescss-clase-por-clase)
9. [`main.cpp` línea a línea conceptual (todas las funciones)](#9-maincpp-todas-las-funciones-y-estructuras)
10. [Fórmulas científicas (con ejemplo numérico)](#10-fórmulas-científicas-con-ejemplo-numérico)
11. [Rutinas y menú: por qué se eligieron así](#11-rutinas-y-menú-por-qué-se-eligieron-así)
12. [Seguridad y validación (preguntas típicas de defensa)](#12-seguridad-y-validación)
13. [Preguntas frecuentes para el jurado](#13-preguntas-frecuentes-para-el-jurado)
14. [Cómo subirlo a un servidor para que todo el mundo lo vea](#14-cómo-subirlo-a-un-servidor-para-que-todo-el-mundo-lo-vea)
15. [Limitaciones honestas](#15-limitaciones-honestas)

---

## 1. Qué es el proyecto y qué problema resuelve

PulseLab es una **aplicación web local (y desplegable en un VPS)** que:

1. Recibe datos antropométricos por un formulario HTML nativo.
2. El servidor C++ **valida** que los números sean lógicos.
3. Calcula **IMC**, **TMB** (Mifflin-St Jeor) y **TDEE** (gasto total).
4. Asigna un **objetivo calórico automático** (déficit, mantenimiento o superávit).
5. Reparte **macronutrientes**.
6. Genera una **rutina** según 3, 4 o 5 días disponibles.
7. Propone un **menú diario** con porciones escaladas a esas calorías.
8. Escribe un archivo de texto `resultados_<nombre>.txt` en disco.
9. Devuelve una **página HTML completa** (SSR: *server-side rendering*) usando el mismo CSS.

No es un diagnóstico médico. El pie de página lo dice en las dos vistas.

---

## 2. Arquitectura (cómo viaja un clic hasta el resultado)

```
Navegador                         Proceso C++ (fitness_app.exe)
─────────                         ─────────────────────────────
GET  /                 ────────►  httplib sirve public/index.html
GET  /styles.css       ────────►  httplib sirve public/styles.css

Usuario pulsa "Calcular mi plan"
POST /calcular
  application/x-www-form-urlencoded
  nombre=...&sexo=F&edad=28&...   ────────►
                                  leerFormulario()
                                  validarUsuario()
                                  calcularPlan()
                                  generarRutina()
                                  generarMenu()
                                  guardarResumen()  → resultados_Ana_Perez.txt
                                  paginaResultados() + layout()
                         ◄──────── HTML 200 (o 400 si hay error)
Navegador pinta la página
  y vuelve a pedir GET /styles.css
```

Puntos que debes recitar:

- **No hay JSON ni AJAX.** El navegador hace un POST clásico de formulario y **cambia de documento**.
- **No hay base de datos.** El estado no se guarda en SQL; cada POST es independiente. El único persistente es el `.txt`.
- **Un solo proceso.** `httplib::Server` abre el puerto **8080** y atiende HTTP/1.1.
- **`listen("0.0.0.0", 8080)`** significa “todas las interfaces de red”, no solo localhost. En un PC de casa, otros dispositivos de la LAN pueden entrar con `http://IP-LOCAL:8080` si el firewall lo permite.

Códigos HTTP que usamos:

| Código | Cuándo |
|---|---|
| 200 | GET de archivos estáticos o POST `/calcular` válido |
| 400 | Datos mal formados o fuera de rango (`paginaError`) |
| 404 | Ruta inexistente; el `error_handler` pinta HTML de error |

---

## 3. Por qué C++ y por qué cero JavaScript

**C++ en el backend**

- Demuestra control de tipos (`struct`, `double`, `int`), validación en servidor y generación de HTML.
- Un binario nativo: no hace falta Node, Python ni PHP en el servidor (sí hace falta el runtime de Windows / libc++ en Linux).
- `cpp-httplib` es **header-only**: se incluye `httplib.h` y se enlaza con sockets (`-lws2_32` en Windows, `-pthread` en Linux).

**Cero JavaScript**

- El envío es `<form action="/calcular" method="POST">`.
- Las barras de macros **no** se animan con JS: el servidor escribe `style="width: 28%;"` en el HTML.
- Imprimir PDF es **Ctrl+P** + `@media print` en CSS, no `window.print()`.
- El botón “Limpiar” es `type="reset"` (comportamiento nativo del navegador).

Si preguntan “¿el navegador no valida también?”: sí, `required`, `min` y `max` en HTML son **primera línea**. El servidor **vuelve a validar** porque se puede enviar un POST con `curl` saltándose el HTML. Eso es un argumento de defensa fuerte (nunca confiar solo en el cliente).

---

## 4. Estructura de carpetas

```
fitness_app/
├── httplib.h              Servidor HTTP (librería de terceros, MIT)
├── main.cpp               TODO el motor: datos, fórmulas, HTML, disco, rutas
├── fitness_app.exe        Binario Windows (se genera al compilar)
├── README.md              Este documento
├── resultados_*.txt       Salida automática por cada cálculo exitoso
└── public/
    ├── index.html         Formulario inicial (único HTML estático)
    └── styles.css         Estilos compartidos (formulario + HTML generado)
```

La carpeta `public/` **debe existir junto al ejecutable** (o debes lanzar el exe **desde la raíz del proyecto**). El código monta `./public`. Si ejecutas el `.exe` desde otra carpeta, fallará el `set_mount_point` y el programa termina con código 1.

---

## 5. Librería `httplib.h`

- Proyecto: [yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib), licencia MIT.
- En este repo ya está el archivo (versión 0.66.0 al momento de crearlo).
- **No se compila aparte.** `main.cpp` hace `#include "httplib.h"` y el compilador pega todo el código del servidor.

En Windows, **antes** del include, `main.cpp` define:

```cpp
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00   // Windows 10 o superior
#endif
#endif
```

Sin eso, httplib se niega a compilar (no soporta Windows 8 o menor).

Para actualizarla:

```bat
curl.exe -L -o httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
```

**Qué usamos de httplib (API concreta):**

| API | Uso |
|---|---|
| `httplib::Server svr` | Crea el servidor |
| `svr.set_mount_point("/", "./public")` | Sirve `index.html` y `styles.css` |
| `svr.Post("/calcular", lambda)` | Ruta de cálculo |
| `req.get_param_value("nombre")` | Lee campos del POST urlencoded |
| `res.set_content(html, "text/html; charset=utf-8")` | Cuerpo de respuesta |
| `res.status = 400` | Error de validación |
| `svr.set_error_handler(...)` | Página 404 |
| `svr.listen("0.0.0.0", 8080)` | Bucle bloqueante hasta Ctrl+C |

---

## 6. Compilar y ejecutar en local

Siempre desde la carpeta que contiene `main.cpp` y `public/`.

### Windows (MinGW-w64 / MSYS2, `g++` en el PATH)

```powershell
g++ -std=c++17 -O2 main.cpp -o fitness_app.exe -lws2_32
.\fitness_app.exe
```

Significado de cada flag:

| Flag | Por qué |
|---|---|
| `-std=c++17` | Lambdas, `auto`, `std::string` moderno; httplib lo exige |
| `-O2` | Optimiza el binario (httplib es grande) |
| `main.cpp` | Único archivo fuente nuestro |
| `-o fitness_app.exe` | Nombre del ejecutable |
| `-lws2_32` | **Winsock2**: sockets TCP en Windows. Sin esto, *undefined reference* a `socket`, `bind`, `listen` |

Abre [http://localhost:8080](http://localhost:8080).

### Linux

```bash
g++ -std=c++17 -O2 main.cpp -o fitness_app -pthread
./fitness_app
```

`-pthread` es obligatorio: httplib crea hilos para cada conexión.

### macOS

Igual que Linux (`-pthread`).

### Qué verás en consola

```
PulseLab escuchando en http://localhost:8080
Abre esa URL en el navegador. Ctrl+C para detener.
```

En Windows, `SetConsoleOutputCP(CP_UTF8)` hace que tildes en `std::cout` no se vean rotas.

---

## 7. `public/index.html` campo por campo

Documento HTML5. **No hay `<script>`.** El CSS se carga con:

```html
<link rel="stylesheet" href="/styles.css">
```

httplib resuelve `/styles.css` → `public/styles.css`.

### Cabecera y layout

- `lang="es"`: accesibilidad e idioma.
- `charset="UTF-8"`: tildes (evaluación, calórico).
- `viewport`: en móvil no se ve “zoomed out”.
- `.page`: contenedor centrado de máximo ~1120 px.
- `.topbar` + `.logo` “PL”: identidad visual.
- `.hero`: texto de bienvenida (no es formulario).
- `.grid-2`: formulario a la izquierda, lista “Qué vas a recibir” a la derecha.
- `footer`: disclaimer médico (obligatorio en defensa: el programa **no prescribe**).

### El formulario (el corazón del frontend)

```html
<form class="form-card" action="/calcular" method="POST">
```

- `action="/calcular"`: URL relativa al mismo host/puerto.
- `method="POST"`: los datos van en el **cuerpo**, no en la barra de direcciones (un GET pondría peso y edad en el historial).

Cada `name="..."` **debe coincidir** con `req.get_param_value("...")` en C++. Si cambias `name="peso"` a `name="weight"` y no tocas C++, el servidor dice que el peso no es un número.

| `name` | Control | Restricciones HTML | Qué espera C++ |
|---|---|---|---|
| `nombre` | text | `maxlength="60"` `required` | string recortado, ≥ 2 caracteres |
| `sexo` | select | `M` / `F` | char `'M'` o `'F'` (se pasa a mayúscula) |
| `edad` | number | min 13 max 99 | int 13–99 |
| `peso` | number | min 21 max 349 step 0.1 | double 20 < x < 350 |
| `altura` | number | min 101 max 249 | double 100 < x < 250 (cm) |
| `dias` | select | 3 / 4 / 5 | int exactamente 3, 4 o 5 |
| `cintura` | number | opcional, 40–200 | si viene vacío, `tiene_cintura = false` |

`label for="..."` + `id="..."`: al hacer clic en el texto se enfoca el input (accesibilidad, sin JS).

Botones:

- `type="submit"` → dispara el POST.
- `type="reset"` → vacía el formulario en el cliente; **no llama al servidor**.

El `aside` de la derecha es solo informativo; no envía datos.

---

## 8. `public/styles.css` clase por clase

El mismo archivo lo usan `index.html` **y** el HTML que imprime C++ (`<link rel="stylesheet" href="/styles.css">` dentro de `layout()`).

### Tokens y tema (`:root`)

Variables CSS (`--bg-0`, `--indigo`, `--text`…). Cambiar el color de marca es tocar una variable, no 40 reglas. Paleta oscura índigo/pizarra + acentos cian, ámbar y esmeralda.

`@import` de Google Fonts **Outfit**: es CSS, no JS. Si no hay red, el navegador cae a `system-ui`.

### Reset y fondo

- `* { box-sizing: border-box; }`: el `padding` no desborda el ancho.
- `body`: tres capas de `radial-gradient` + `linear-gradient` (glow índigo/cian).

### Componentes de layout

| Clase | Función |
|---|---|
| `.page` | Ancho fluido: `min(1120px, 100% - 32px)` |
| `.topbar` | Flex: logo a la izquierda, chip a la derecha |
| `.brand` / `.logo` | Identidad; el logo es un cuadrado con gradiente, no una imagen |
| `.chip` | Pastilla “Sin JavaScript…” |
| `.hero`, `.card`, `.form-card`, `.alert` | Tarjetas: borde suave, sombra, `backdrop-filter` |
| `.hero::after` | Círculo decorativo (pseudo-elemento, cero HTML extra) |
| `.kicker` | Etiqueta en versales (PLANIFICADOR EDUCATIVO) |
| `.grid-2` | 1.15fr + 0.85fr: formulario más ancho que el aside |
| `.form-grid` | Campos en 2 columnas |
| `.field.full` | `grid-column: 1 / -1` el nombre ocupa todo el ancho |
| `.btn-primary` / `.btn-ghost` | CTA vs secundario; también se usan como `<a>` |
| `.checklist` | Lista sin viñetas, cada ítem es una mini-tarjeta |
| `.metrics` | 4 columnas: IMC, TMB, TDEE, objetivo |
| `.badge.ok/.warn/.alert` | Color según clasificación de IMC |
| `.note` | Cajas de advertencia (masa muscular, cintura) |
| `.macros` / `.macro-row` / `.bar` | Barras de porcentaje |
| `.protein` `.fat` `.carb` | Color de cada barra |
| `.days` | `auto-fit minmax(280px, 1fr)`: N tarjetas de días |
| `.table` | Ejercicios: nombre, series, descanso |
| `.meals` | 2 columnas: 4 comidas |
| `.alert` | Página de error centrada |
| `.footer` | Disclaimer |
| `.print-tip` / `.no-print` | Se ocultan al imprimir |

### Barras de macros (pregunta típica: “¿eso no es JS?”)

HTML generado:

```html
<div class="bar"><span class="protein" style="width: 28.0%;"></span></div>
```

El `span` es `display:block; height:100%`. El **ancho** lo calcula C++ (`pct_proteina`) y lo incrusta. CSS solo pinta. Cero JavaScript.

### Responsive (`@media (max-width: 900px)`)

En pantallas estrechas, grids de 2/4 columnas pasan a 1. El topbar y los botones se apilan (`flex-direction: column`).

### Impresión (`@media print`)

- Fondo blanco, texto negro (ahorra tinta y es legible en PDF).
- Oculta `.no-print`, chip, tip de Ctrl+P y botones.
- `break-inside: avoid` en tarjetas para no cortar un día de gym a la mitad de página.

El usuario no necesita un botón “Exportar PDF”: el navegador ya sabe imprimir.

---

## 9. `main.cpp`: todas las estructuras y funciones

Archivo único de lógica (~685 líneas). `static` delante de funciones = visibilidad interna al archivo (como “privado” de un módulo).

### 9.1 Prefijo Windows y includes

Orden importante:

1. `_WIN32_WINNT 0x0A00` **antes** de `httplib.h`.
2. `httplib.h`.
3. STL: `algorithm` (trim), `cctype` (isspace, toupper), `cmath` (round), `fstream` (archivo), `iomanip` (decimales), `iostream` (consola), `sstream` (parseo y HTML), `string`, `vector`.

### 9.2 Estructuras de datos

Se usan `struct` (agregados públicos) porque son DTOs, no clases con invariantes complejos. En defensa: “separé el modelo de la vista y de las rutas”.

**`Usuario`** — entrada ya parseada:

| Campo | Tipo | Significado |
|---|---|---|
| `nombre` | `std::string` | Texto libre (luego se escapa al HTML) |
| `sexo` | `char` | `'M'` o `'F'` (Mifflin-St Jeor cambia la constante) |
| `edad` | `int` | Años |
| `peso` | `double` | kg |
| `altura_cm` | `double` | cm (el IMC convierte a metros) |
| `dias_entrenamiento` | `int` | 3, 4 o 5: TDEE **y** tipo de rutina |
| `cintura_cm` | `double` | Opcional |
| `tiene_cintura` | `bool` | Distingue “0 cm” de “no lo rellenó” |

**`PlanNutricional`** — toda la salida numérica: IMC, textos, TMB, factor, TDEE, objetivo, macros en gramos y en **porcentaje de calorías** (para las barras), nota de cintura.

**`Ejercicio`**: nombre, series/reps, descanso.

**`DiaEntrenamiento`**: título + enfoque + vector de ejercicios.

**`Rutina`**: nombre del split + descripción + vector de días.

**`Comida`**: momento (Desayuno…), plato, detalle con gramos.

**`Validacion`**: `{ ok, mensaje }`. Evita excepciones para errores de usuario (un 400 es un flujo normal).

### 9.3 `trim`

Quita espacios al inicio y al final. Necesario porque un POST puede traer `"  80  "`. Usa `std::isspace` con `unsigned char` (regla de C++: no pasar `char` negativo a `<cctype>`).

### 9.4 `htmlEscape` — defensa XSS

Si alguien pone de nombre `<script>alert(1)</script>`, **no hay JS nuestro**, pero el navegador interpretaría etiquetas si las concatenamos crudas.

Sustituciones:

| Carácter | Entidad |
|---|---|
| `&` | `&amp;` (primero, o rompería el resto) |
| `<` | `&lt;` |
| `>` | `&gt;` |
| `"` | `&quot;` |
| `'` | `&#39;` |

Se aplica a **todo texto que viene del usuario** (y a textos internos por consistencia) antes de meterlos en HTML.

### 9.5 `parseDouble` / `parseInt`

No usan `atoi` a ciegas. `std::istringstream` + `eof()`: `"80abc"` falla. Cadena vacía falla. Así distinguimos “no mandó el campo” de “mandó basura”.

### 9.6 `format1` / `format0`

`std::fixed` + `setprecision(1)` o `0`. El HTML y el `.txt` muestran `26.1` y `2297`, no `26.122448...`.

### 9.7 `sanitizarNombreArchivo`

El nombre entra en `resultados_<nombre>.txt`. Si alguien pone `../../windows/system32` o `Ana Pérez`, se dejan letras, dígitos, `_` y `-`; espacios → `_`. Vacío → `"usuario"`. Máximo 40 caracteres. **No es un path traversal** usable.

### 9.8 `layout(title, body, print_tip)`

Plantilla HTML común: doctype, charset, viewport, link a `/styles.css`, topbar PulseLab, opcional tip de Ctrl+P, el `body` que le pases, footer médico.

`paginaError` y `paginaResultados` **no** duplican el cascarón: solo el interior. Eso es “composición de vistas” en C++.

### 9.9 `paginaError`

Tarjeta `.alert`, mensaje escapado, enlace `/` (GET del formulario). HTTP 400 lo pone la ruta, no esta función.

### 9.10 `validarUsuario`

Rangos **lógicos**, no solo “es un número”:

| Campo | Regla | Por qué |
|---|---|---|
| nombre | ≥ 2 | Evita vacíos / una letra |
| sexo | M o F | Mifflin-St Jeor está tabulada así |
| edad | 13–99 | Fuera, las fórmulas no están pensadas para niños pequeños |
| peso | 20 < kg < 350 | Evita 0, negativos, basura |
| altura | 100 < cm < 250 | El IMC con 50 cm explota o da valores absurdos |
| días | 3, 4 o 5 | Fuera no hay rutina ni factor |
| cintura | 40–200 **solo si** `tiene_cintura` | No castigar el campo opcional vacío |

### 9.11 `factorActividad`

Mapeo de **días de gym → multiplicador Harris-Benedict/TDEE clásico**:

| Días | Factor | Interpretación usual |
|---|---|---|
| 3 | 1.375 | Ligero (1–3 sesiones) |
| 4 | 1.55 | Moderado (3–5) |
| 5 | 1.725 | Alto (casi diario) |

Es una **estimación poblacional**, no un acelerómetro.

### 9.12 `calcularPlan` — el motor

Paso a paso (mismo orden que el código):

1. `altura_m = altura_cm / 100`.
2. `IMC = peso / (altura_m * altura_m)`.
3. Clasificación OMS aproximada:
   - `< 18.5` Bajo peso → **+400 kcal** (superávit)
   - `< 25` Peso normal → **0** (mantenimiento / recomposición)
   - `< 30` Sobrepeso → **−450 kcal**
   - `≥ 30` Obesidad → **−450 kcal**
4. Nota fija: el IMC **no distingue músculo de grasa**.
5. TMB Mifflin-St Jeor:
   - Hombre: `10*peso + 6.25*altura_cm - 5*edad + 5`
   - Mujer: `10*peso + 6.25*altura_cm - 5*edad - 161`
6. `TDEE = TMB * factor`.
7. `calorias_objetivo = TDEE + ajuste`, con **suelo de 1200 kcal** (no generar dietas extremas por software).
8. Macros:
   - Proteína `2.0 g * kg` × 4 kcal/g
   - Grasa `0.8 g * kg` × 9 kcal/g
   - Carbos = calorías que quedan / 4; si sale negativo, se clava en 0
9. Porcentajes sobre la suma real de kcal de macros (para las barras).
10. Si hay cintura:
    - WHtR = cintura_cm / altura_cm. Umbral educativo **0.5**.
    - OMS cintura: hombre ≥ 94 cm, mujer ≥ 80 cm → frase extra.

### 9.13 `generarRutina`

Lambda local `dia(...)` para no repetir el relleno de `DiaEntrenamiento`.

- **3 días:** Full Body A/B/C (compuestos + un día más unilateral/core). Descanso entre sesiones.
- **4 días:** Torso A / Pierna A / Torso B / Pierna B.
- **5 días (else):** Push / Pull / Legs / Upper / Lower.

Cada ejercicio: nombre, series×reps, descanso en segundos. Son plantillas de hipertrofia/fuerza general, no periodización avanzada.

### 9.14 `generarMenu`

Base de diseño: menú pensado ~**2200 kcal**.  
`factor = calorias_objetivo / 2200`.  
Lambda `g(base)` = `max(20, round(base * factor))` gramos, para no sugerir 8 g de avena.

Cuatro comidas: Desayuno, Almuerzo, Merienda, Cena. Intercambiables (pollo/pescado/tofu).

### 9.15 `guardarResumen`

`std::ofstream` en modo texto. Si el disco falla, devuelve `false` y la web **igual** muestra el plan (el archivo es extra, no bloquea).

Contenido: datos, IMC, TMB, TDEE, macros, rutina línea a línea, menú, aviso legal.

### 9.16 `paginaResultados`

Construye el HTML de éxito:

1. Hero con nombre escapado e IMC.
2. Botón “Nueva evaluación” → `href="/"` clase `no-print`.
3. Cuatro métricas; el badge de IMC:
   - `ok` (normal), `warn` (bajo o sobrepeso), `alert` (obesidad).
4. Notas IMC y cintura.
5. Tarjeta de macros con `style="width: XX%;"` (inline, requisito del enunciado).
6. Loop de días → tablas.
7. Loop de comidas.
8. Nombre del archivo si se pudo guardar.
9. `layout(..., print_tip=true)`.

### 9.17 `leerFormulario`

Traduce POST → `Usuario`. Orden:

1. `nombre` + `trim`.
2. `sexo`: primer carácter en mayúscula.
3. edad, peso, altura, días: si el parseo falla, `val.ok = false` y mensaje específico.
4. cintura: si el string está vacío, se ignora; si hay texto inválido, error.

**Doble validación:** parseo (tipo) + `validarUsuario` (rango).

### 9.18 `main`

1. UTF-8 en consola Windows.
2. `httplib::Server`.
3. Montar `./public` o salir con error.
4. Registrar POST `/calcular` (pipeline completo).
5. 404 personalizado.
6. `listen 0.0.0.0:8080` — **bloquea** el hilo principal hasta Ctrl+C.

El lambda del POST es el “controller”:

```
leerFormulario → ¿parse ok?
validarUsuario → ¿rangos ok?
calcularPlan
generarRutina
generarMenu
guardarResumen
paginaResultados → 200
```

Cualquier fallo temprano: `400` + `paginaError`.

---

## 10. Fórmulas científicas (con ejemplo numérico)

Ejemplo real usado en pruebas: mujer, 28 años, 80 kg, 175 cm, 5 días, cintura 88 cm.

**IMC**

```
altura_m = 175 / 100 = 1.75
IMC = 80 / (1.75²) = 80 / 3.0625 ≈ 26.1  → Sobrepeso → −450 kcal
```

**TMB (mujer)**

```
TMB = 10*80 + 6.25*175 - 5*28 - 161
    = 800 + 1093.75 - 140 - 161
    = 1592.75 kcal
```

**TDEE (5 días, 1.725)**

```
TDEE ≈ 1592.75 * 1.725 ≈ 2747 kcal
objetivo = 2747 - 450 ≈ 2297 kcal
```

**Macros**

```
proteína = 2.0 * 80 = 160 g → 640 kcal
grasa    = 0.8 * 80 = 64 g  → 576 kcal
carbos   = (2297 - 640 - 576) / 4 ≈ 270 g → 1081 kcal
% proteína ≈ 640/2297 ≈ 28%
```

**Cintura**

```
WHtR = 88 / 175 = 0.503 ≥ 0.5
Mujer ≥ 80 cm → criterio OMS de atención
```

Memoriza: **Mifflin-St Jeor (1990)** es más usada hoy que Harris-Benedict para TMB en software.

---

## 11. Rutinas y menú: por qué se eligieron así

| Días | Split | Justificación breve |
|---|---|---|
| 3 | Full Body | Frecuencia alta por músculo cuando hay poco tiempo |
| 4 | Torso/Pierna | Volumen extra de pierna vs un “upper/lower” clásico |
| 5 | PPL + Upper/Lower | Mucho volumen; el upper/lower del final es más ligero |

El menú no es una dieta clínica: es una **plantilla educativa** que escala gramos. Un nutricionista ajustaría alergias, presupuesto y preferencias.

---

## 12. Seguridad y validación

Qué sí hace el proyecto:

- Validación de tipos y rangos **en servidor**.
- Escape HTML (XSS de nombres).
- Sanitizado del nombre de archivo.
- `charset=utf-8` explícito.
- Sin secretos ni API keys.

Qué no hace (sé honesto en la defensa):

- No hay login ni HTTPS en el binario (HTTPS se pone con un proxy Nginx + Let’s Encrypt, ver sección 14).
- El `.txt` se escribe en el disco del servidor (privacidad: no es adecuado para un hospital).
- httplib es un servidor embebido, no un reemplazo de Nginx para millones de usuarios.
- No hay CSRF token (app educativa; en producción se añadiría).

---

## 13. Preguntas frecuentes para el jurado

**¿Por qué no PHP / Node?**  
El requisito era demostrar C++ sirviendo HTTP y generando la vista.

**¿Dónde está la “base de datos”?**  
No hay. Es un motor de cálculo. Persistencia mínima: archivo de texto.

**¿El IMC de un culturista dirá obesidad?**  
Sí, y por eso está la nota de masa muscular. El código no usa % grasa ni pliegues.

**¿Por qué 2.0 g/kg de proteína?**  
Rango habitual de literatura de entrenamiento de fuerza (aprox. 1.6–2.2 g/kg). Se fijó 2.0 para simplicidad.

**¿Qué pasa si proteína + grasa superan las calorías?**  
Los carbos quedan en 0. El suelo de 1200 kcal reduce (no elimina) ese caso.

**¿Cómo se ve en el móvil?**  
CSS Grid/Flex + media query 900 px. Sin JS de menú hamburguesa: no hace falta.

**¿Puedo hacer trampa con curl?**  
Sí, y el servidor igual valida. Eso demuestra que la seguridad no vive en el HTML.

**¿Por qué `0.0.0.0` y no `127.0.0.1`?**  
`127.0.0.1` solo la máquina local. `0.0.0.0` permite LAN y, detrás de un firewall/NAT, Internet.

---

## 14. Cómo subirlo a un servidor para que todo el mundo lo vea

### 14.1 Idea clave (la más importante)

PulseLab **no es un sitio estático**. GitHub Pages, Netlify o un hosting “solo HTML” **no sirven**: no ejecutan C++ ni aceptan `POST /calcular`.

Necesitas un **ordenador que esté encendido 24/7**, con el binario compilado, la carpeta `public/` al lado, y el **puerto 8080 (o 80/443 con proxy)** accesible.

Opciones reales, de más casera a más profesional:

1. Túnel temporal (ngrok) — demo en clase.
2. PC de casa + redirección de puertos — frágil.
3. **VPS Linux (la opción correcta para “que todos lo vean”)**.
4. PaaS (Railway, Render, Fly.io) con Dockerfile.

---

### 14.2 Demo rápida en clase (sin comprar servidor): ngrok

En tu PC, con el exe ya corriendo en 8080:

1. Crea cuenta en [ngrok](https://ngrok.com) e instala el cliente.
2. `ngrok http 8080`
3. Te da una URL `https://algo.ngrok-free.app`.

Cualquiera con ese enlace ve tu app. Al cerrar el PC o ngrok, se acaba. El plan gratis cambia la URL cada vez.

---

### 14.3 VPS Linux (recomendado para producción)

Proveedores habituales: DigitalOcean, Hetzner, Oracle Cloud Free Tier, AWS Lightsail, Contabo, un VPS de Hostinger/Namecheap. Eliges **Ubuntu 22.04/24.04**, 1 vCPU, 1 GB RAM basta.

#### A. Conectar y copiar el proyecto

Desde tu Windows (PowerShell), con la IP pública del VPS:

```powershell
scp -r E:\Fabian\Escritorio\fitness_app usuario@TU_IP:/home/usuario/fitness_app
ssh usuario@TU_IP
```

(O sube un ZIP y descomprimes. O `git clone` si el repo está en GitHub.)

#### B. Instalar compilador y compilar **en Linux**

No copies `fitness_app.exe` de Windows al VPS: es otro sistema operativo.

```bash
sudo apt update
sudo apt install -y g++ build-essential
cd ~/fitness_app
g++ -std=c++17 -O2 main.cpp -o fitness_app -pthread
./fitness_app
```

Prueba: `http://TU_IP:8080` (abre el puerto 8080 en el firewall del proveedor y en `ufw`).

```bash
sudo ufw allow 8080/tcp
sudo ufw allow OpenSSH
sudo ufw enable
```

#### C. Que no se muera al cerrar SSH: systemd

Crea `/etc/systemd/system/pulselab.service`:

```ini
[Unit]
Description=PulseLab C++ web app
After=network.target

[Service]
Type=simple
User=usuario
WorkingDirectory=/home/usuario/fitness_app
ExecStart=/home/usuario/fitness_app/fitness_app
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

`WorkingDirectory` es **crítico**: ahí debe estar `public/`.

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now pulselab
sudo systemctl status pulselab
```

#### D. Dominio + HTTPS (Nginx como reverse proxy)

El binario sigue en 8080 **solo en localhost**. Nginx escucha 80 y 443 y reenvía.

```bash
sudo apt install -y nginx
```

`/etc/nginx/sites-available/pulselab`:

```nginx
server {
    listen 80;
    server_name tudominio.com www.tudominio.com;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        client_max_body_size 1m;
    }
}
```

```bash
sudo ln -s /etc/nginx/sites-available/pulselab /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
```

En el registrador de dominio (Namecheap, Cloudflare, etc.) crea un **registro A** apuntando a `TU_IP`.

Certificado gratis:

```bash
sudo apt install -y certbot python3-certbot-nginx
sudo certbot --nginx -d tudominio.com -d www.tudominio.com
```

A partir de ahí la gente entra a `https://tudominio.com` sin poner `:8080`.

Cierra 8080 al mundo y deja solo 80/443:

```bash
sudo ufw delete allow 8080/tcp
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
```

El servicio C++ escucha `0.0.0.0:8080`; con el firewall, solo Nginx (misma máquina) debería usarlo. Aún más estricto: podrías cambiar el código a `listen("127.0.0.1", 8080)` para que **nunca** sea alcanzable desde Internet salvo por el proxy.

#### E. Qué no olvidar en el VPS

- Los `resultados_*.txt` se crean en `WorkingDirectory`. En un servidor público **mezclan datos de desconocidos**. Para una demo está bien; para producción real habría que desactivar `guardarResumen` o guardar por usuario autenticado.
- Reinicios: `Restart=always` en systemd.
- Actualizar código: `git pull` (o scp), recompilar, `sudo systemctl restart pulselab`.

---

### 14.4 Docker (útil en Railway / Fly.io / un VPS)

Archivo `Dockerfile` que puedes crear en la raíz (aún no está en el repo; es la receta):

```dockerfile
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y g++ && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY httplib.h main.cpp ./
COPY public ./public
RUN g++ -std=c++17 -O2 main.cpp -o fitness_app -pthread
EXPOSE 8080
CMD ["./fitness_app"]
```

```bash
docker build -t pulselab .
docker run --rm -p 8080:8080 pulselab
```

En Railway/Render/Fly: conectas el GitHub, usas este Dockerfile, publican ellos una URL `https://pulselab-xxxx.up.railway.app`.

---

### 14.5 Por qué **no** funciona “subir index.html a Hostinger compartido”

El hosting PHP típico no compiló tu `main.cpp`. Aunque subas `index.html`, el `POST /calcular` caerá en 404 del Apache ajeno. Necesitas **ejecutar el binario**.

---

### 14.6 Red local (sin Internet)

1. `ipconfig` (Windows) → IPv4 tipo `192.168.1.15`.
2. Servidor en marcha.
3. Firewall de Windows: regla de entrada TCP 8080.
4. En el móvil (misma Wi‑Fi): `http://192.168.1.15:8080`.

Útil en una defensa presencial si falla Internet.

---

## 15. Limitaciones honestas

- Estimaciones poblacionales (TMB, factores 1.375/1.55/1.725), no calorimetría.
- Sexo binario M/F porque así está la ecuación de Mifflin-St Jeor en su forma clásica.
- Cintura opcional mejora el **contexto**, no recalcula las calorías (el enunciado pedía “mayor precisión” informativa).
- Un proceso, un puerto, sin balanceador.
- httplib no está pensado como CDN mundial.
- Herramienta **educativa**: el footer no es adorno, es parte del alcance ético del proyecto.

---

## Mapa rápido “me preguntan X → abro Y”

| Pregunta | Archivo | Símbolo |
|---|---|---|
| ¿Dónde nace el POST? | `public/index.html` | `<form action="/calcular">` |
| ¿Dónde se escuchan las peticiones? | `main.cpp` | `main()`, `svr.Post`, `listen` |
| ¿Dónde se validan rangos? | `main.cpp` | `validarUsuario` |
| ¿IMC / TMB / macros? | `main.cpp` | `calcularPlan` |
| ¿Quién decide Full Body vs PPL? | `main.cpp` | `generarRutina` |
| ¿Por qué cambian los gramos de avena? | `main.cpp` | `generarMenu` |
| ¿Dónde se escribe el .txt? | `main.cpp` | `guardarResumen` |
| ¿Cómo se arma el HTML de resultados? | `main.cpp` | `paginaResultados` + `layout` |
| ¿XSS? | `main.cpp` | `htmlEscape` |
| ¿Colores y print? | `public/styles.css` | `:root`, `@media print` |
| ¿Barras sin JS? | `main.cpp` + CSS | `style="width: XX%;"` + `.bar` |

Con esto puedes defender el flujo completo: del `name="peso"` del HTML hasta el `double peso` del `struct Usuario`, la fórmula, la barra CSS y el archivo en disco.
