// ============================================================
//  ChargeGrid Intelligence — Protótipo ESP32
//  Equipe ARM | FIAP 2026 | Parceiro: GoodWe
//  Integrantes:
//    Miguel Piedade  — RM572445
//    Arthur Oliveira — RM568986
//    Rafael Zani     — RM569033
//
//  Funcionalidades simuladas:
//    1. Peak Shaving Dinâmico
//    2. PV-to-EV (Energia limpa GoodWe)
//    3. Telemetria em tempo real via Blynk IoT
//    4. Display LCD com status de baixo nível
// ============================================================

#define BLYNK_TEMPLATE_ID "TMPL2Eyluy7kJ"
#define BLYNK_TEMPLATE_NAME "goodwe sustentabilidade"
#define BLYNK_AUTH_TOKEN "ZkceGqRgMRVTuiRdjSqBwx6ZSI6hU6aM"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <LiquidCrystal_I2C.h>

// --- Protótipos ---
void atualizaLCD();
void leituraTelemetria();

// --- WiFi ---
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// --- Pinos ---
const int PINO_RELE   = 12;   // Relé → simula contactor do carregador
const int PINO_SENSOR = 34;   // Potenciômetro → simula sensor Hall de corrente

// --- Estado do sistema ---
bool releEstado  = false;  // true = carregando | false = parado/seguro
bool modoSolar   = false;  // true = energia PV-to-EV | false = rede elétrica

// --- Parâmetros elétricos ---
const float TENSAO_REDE      = 230.0;  // Volts (padrão europeu/comercial)
const float LIMITE_CORRENTE  = 32.0;   // Ampères — limite do carregador
const float LIMITE_PEAKSHAVE = 25.0;   // Ampères — gatilho de Peak Shaving
const float CORRENTE_REDUZIDA = 16.0;  // Ampères — potência reduzida após peak shaving
const float PRECO_KWH_REDE   = 0.95;   // R$/kWh — tarifa média SP (rede elétrica)
const float PRECO_KWH_SOLAR  = 0.12;   // R$/kWh — custo marginal energia solar local

// --- Acumuladores de sessão ---
float energiaAcumulada = 0.0;  // kWh consumidos na sessão atual
bool  peakShavingAtivo = false; // indica se o sistema está modulando a potência

LiquidCrystal_I2C lcd(0x27, 16, 2);
BlynkTimer timer;

// ============================================================
//  BLYNK: Controle de Autorização de Recarga (V1)
// ============================================================
BLYNK_WRITE(V1) {
  releEstado = param.asInt();
  digitalWrite(PINO_RELE, releEstado ? HIGH : LOW);

  if (!releEstado) {
    // Sessão encerrada — reseta acumuladores
    energiaAcumulada  = 0.0;
    peakShavingAtivo  = false;
    Blynk.virtualWrite(V3, 0);
    Blynk.virtualWrite(V4, 0);
    Blynk.virtualWrite(V5, 0); // Peak Shaving flag
  }
  atualizaLCD();
}

// ============================================================
//  BLYNK: Modo PV-to-EV (V6)
// ============================================================
BLYNK_WRITE(V6) {
  modoSolar = param.asInt();
  atualizaLCD();
}

// ============================================================
//  LCD: Atualiza linha de status
// ============================================================
void atualizaLCD() {
  lcd.setCursor(0, 1);
  if (!releEstado) {
    lcd.print("Status: OFF/SAFE");
  } else if (peakShavingAtivo) {
    lcd.print("PeakShave: 16A  ");
  } else if (modoSolar) {
    lcd.print("Modo: PV-to-EV  ");
  } else {
    lcd.print("Status: CARREGND");
  }
}

// ============================================================
//  TELEMETRIA E REGRAS DE NEGÓCIO
// ============================================================
void leituraTelemetria() {
  int   raw = analogRead(PINO_SENSOR);
  float correnteMedida = (raw / 4095.0) * 40.0;

  if (!releEstado) correnteMedida = 0.0;

  // 1. Proteção de Sobrecorrente (32A)
  if (correnteMedida > LIMITE_CORRENTE && releEstado) {
    releEstado = false;
    digitalWrite(PINO_RELE, LOW);
    Blynk.virtualWrite(V1, 0);

    lcd.clear();
    lcd.print("!!! OVERLOAD !!!");
    lcd.setCursor(0, 1);
    lcd.print("A > 32A LIMIT");
    return;
  }

  // 2. Peak Shaving Dinâmico
  float correnteFinal = correnteMedida;

  if (correnteMedida > LIMITE_PEAKSHAVE && releEstado) {
    correnteFinal    = CORRENTE_REDUZIDA;
    peakShavingAtivo = true;
    Blynk.virtualWrite(V5, 1);
  } else {
    peakShavingAtivo = false;
    Blynk.virtualWrite(V5, 0);
  }

  // Cálculos energéticos
  float potencia    = TENSAO_REDE * correnteFinal;
  energiaAcumulada += (potencia / 3600000.0);      
  float precoAtual  = modoSolar ? PRECO_KWH_SOLAR : PRECO_KWH_REDE;
  float custoTotal  = energiaAcumulada * precoAtual;

  // ============================================================
  //  RENDERIZAÇÃO DE BAIXO NÍVEL DO LCD
  // ============================================================
  char bufferLinha0[17]; // 16 caracteres + terminador nulo ('\0')
  snprintf(bufferLinha0, sizeof(bufferLinha0), "I:%4.1fA P:%4.2fkW", correnteFinal, (potencia / 1000.0));
  
  lcd.setCursor(0, 0);
  lcd.print(bufferLinha0);
  
  atualizaLCD(); // Atualiza a linha 1

  // Telemetria nuvem
  Blynk.virtualWrite(V0, correnteFinal);
  Blynk.virtualWrite(V2, potencia);
  Blynk.virtualWrite(V3, energiaAcumulada);
  Blynk.virtualWrite(V4, custoTotal);
}

// ============================================================
//  GERENCIAMENTO DE CONEXÃO DESACOPLADO
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PINO_RELE, OUTPUT);
  digitalWrite(PINO_RELE, LOW);

  lcd.init();
  lcd.backlight();
  lcd.print("ChargeGrid v1.0");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando rede..");

  // Conexão WiFi explícita (Não-bloqueante)
  WiFi.begin(ssid, pass);
  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED && retry_count < 20) {
    delay(500);
    retry_count++;
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    // Configura Blynk via porta 80 sem travar o boot
    Blynk.config(BLYNK_AUTH_TOKEN, "blynk.cloud", 80);
    lcd.print("GoodWe Online   ");
  } else {
    lcd.print("Modo Offline    ");
  }
  
  atualizaLCD();
  timer.setInterval(1000L, leituraTelemetria);
}

void loop() {
  // Executa o pipeline do Blynk apenas se a rede estiver operacional
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
  timer.run();
}