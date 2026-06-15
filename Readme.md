# ⚡ ChargeGrid Intelligence — Sprint 2

> **EV Challenge 2026 | FIAP | Parceiro: GoodWe**  
> Plataforma inteligente de gerenciamento de recarga de veículos elétricos com integração solar.

**Equipe ARM**

| Nome | RM |
|---|---|
| Miguel Piedade | 572445 |
| Arthur de Oliveira | 568986 |
| Rafael Zani | 569033 |

---

## 📋 Índice

1. [Sobre o Projeto](#sobre-o-projeto)
2. [Arquitetura do Sistema](#arquitetura-do-sistema)
3. [Funcionalidades do Protótipo](#funcionalidades-do-protótipo)
4. [Componentes e Justificativas Técnicas](#componentes-e-justificativas-técnicas)
5. [Sustentabilidade e Energias Renováveis](#sustentabilidade-e-energias-renováveis)
6. [Diagrama de Hardware](#diagrama-de-hardware)
7. [Mapa de Pinos](#mapa-de-pinos)
8. [Configuração do Blynk IoT](#configuração-do-blynk-iot)
9. [Como Executar](#como-executar)
10. [Dados Gerados pelo Sistema](#dados-gerados-pelo-sistema)
11. [Vídeo de Demonstração](#vídeo-de-demonstração)

---

## Sobre o Projeto

O **ChargeGrid Intelligence** é uma plataforma de software em nuvem integrada ao ecossistema solar da GoodWe para gerenciar a recarga de veículos elétricos em ambientes comerciais (B2B).

### Problema que resolve

| Problema | Impacto |
|---|---|
| Sobrecarga às 18h com carros carregando no pico de consumo | Risco de apagão predial e acionamento de termelétricas |
| Desperdício de energia solar gerada no meio do dia | Custo de oportunidade e emissões desnecessárias |
| Falta de controle de demanda nos carregadores | Multas por estouro de demanda contratada |

### Solução

1. **Peak Shaving Dinâmico** — monitora a corrente em tempo real e reduz automaticamente a potência dos carregadores quando a demanda do prédio atinge o limite, evitando sobrecarga da rede.
2. **PV-to-EV** — direciona a energia solar excedente gerada pelo inversor GoodWe diretamente para as baterias dos veículos elétricos.
3. **Telemetria em tempo real** — exibe corrente, potência, energia acumulada e custo da sessão no dashboard Blynk IoT.

---

## Arquitetura do Sistema

```
┌─────────────────────────────────────────────────────────────┐
│                    CAMADA DE CAMPO (IoT)                    │
│                                                             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │ Potenciômetro│    │    ESP32     │    │  LCD 16x2    │  │
│  │ (Sensor Hall │───▶│  (Firmware   │───▶│  (Display    │  │
│  │  Simulado)   │    │  ChargeGrid) │    │   Local)     │  │
│  └──────────────┘    └──────┬───────┘    └──────────────┘  │
│                             │                               │
│                      ┌──────▼───────┐                       │
│                      │    Relé      │                       │
│                      │ (Contactor   │                       │
│                      │  Simulado)   │                       │
│                      └──────────────┘                       │
└─────────────────────────┬───────────────────────────────────┘
                          │ WiFi / MQTT (TCP port 80)
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                  CAMADA DE NUVEM (Blynk IoT)                │
│                                                             │
│   V0: Corrente (A)      V2: Potência (W)                   │
│   V3: Energia (kWh)     V4: Custo (R$)                     │
│   V5: Peak Shaving      V6: Modo PV-to-EV                  │
│                                                             │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                CAMADA DE APLICAÇÃO (Dashboard)              │
│                                                             │
│   Operador monitora e controla sessões de recarga           │
│   em tempo real pelo app Blynk no smartphone               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Funcionalidades do Protótipo

### 1. Peak Shaving Dinâmico ⚡

O sistema monitora continuamente a corrente de carga (lida pelo sensor Hall simulado pelo potenciômetro). Quando a corrente ultrapassa **25A** — limite configurado de demanda do prédio — o ChargeGrid reduz automaticamente a potência do carregador para **16A**, modulando a carga sem interromper a sessão.

```
Corrente lida > 25A → Modula para 16A → Indicador V5 acende no app
Corrente lida > 32A → Desliga relé (proteção absoluta)
```

No cenário real, essa modulação é feita via protocolo **OCPP 2.0.1** enviando o comando `SetChargingProfile` ao carregador. No protótipo, simulamos o efeito truncando a variável de corrente e atualizando o dashboard.

### 2. PV-to-EV — Integração Solar ☀️

Quando o switch **V6** é ativado no app, o sistema entra em modo solar:
- O preço por kWh cai de **R$ 0,95** (rede elétrica) para **R$ 0,12** (custo marginal solar)
- O display LCD exibe `Modo: PV-to-EV`
- O custo acumulado da sessão é recalculado com a tarifa solar

Isso simula o redirecionamento da energia excedente do inversor GoodWe para os carregadores EV, que no sistema real é controlado pela API GoodWe SEMS via leitura de geração instantânea.

### 3. Proteção de Sobrecorrente 🔴

Se a corrente ultrapassar **32A** (limite absoluto do carregador, padrão IEC 61851), o relé é desligado imediatamente, o LCD exibe `!!! OVERLOAD !!!` e o app é notificado. Simula o disjuntor de proteção do EVSE.

### 4. Telemetria em Tempo Real 📊

A cada **1 segundo**, o ESP32 calcula e envia ao Blynk:

| Virtual Pin | Dado | Unidade |
|---|---|---|
| V0 | Corrente instantânea | A |
| V2 | Potência instantânea | W |
| V3 | Energia acumulada na sessão | kWh |
| V4 | Custo total da sessão | R$ |
| V5 | Peak Shaving ativo | 0/1 |

---

## Componentes e Justificativas Técnicas

| Componente | Papel no Protótipo | Equivalente Real |
|---|---|---|
| **ESP32** | Controlador central — lê sensores, executa lógica de Peak Shaving e comunica com a nuvem via WiFi/MQTT | EVSE Controller com módulo de comunicação OCPP |
| **Potenciômetro** | Simula sensor Hall de corrente (ex: ACS712/ACS758) — gera valores analógicos de 0 a 40A | Sensor Hall de efeito instalado no cabo do carregador |
| **Módulo Relé** | Simula o contactor eletromecânico que corta o fornecimento de energia ao veículo | Contactor trifásico do carregador AC |
| **LCD I²C 16x2** | Display local de status da sessão — corrente, potência e estado do sistema | Display embutido no carregador (HMI local) |
| **Blynk IoT** | Dashboard de telemetria e controle remoto via nuvem | CSMS (Charge Station Management System) com painel de operação |

### Por que ESP32?

O ESP32 foi escolhido por ter WiFi e Bluetooth integrados, dois núcleos de processamento e ADC de 12 bits — suficiente para simular a leitura analógica do sensor Hall com resolução de ~10mA por step. Em um sistema real, o controlador do carregador rodaria firmware compatível com OCPP 2.0.1.

---

## Sustentabilidade e Energias Renováveis

### Princípio 1 — Gestão de Demanda (Peak Shaving)

A lógica de Peak Shaving implementada no firmware é a representação técnica central da proposta do ChargeGrid. Ao limitar a corrente de carga a **16A** durante períodos de alta demanda, o sistema:

- **Evita o acionamento de usinas termelétricas** pela distribuidora, que são chamadas para cobrir os picos de consumo nas redes comerciais;
- **Reduz a emissão de CO₂** associada à geração de pico, que no Brasil é majoritariamente fóssil (gás natural e óleo combustível);
- **Elimina multas** por ultrapassagem de demanda contratada, que podem representar até 3x o valor da demanda excedida na tarifa verde/azul da ANEEL.

### Princípio 2 — PV-to-EV (Energia Solar Direta)

O modo PV-to-EV simula a funcionalidade mais relevante da integração ChargeGrid + GoodWe: usar a energia gerada pelos painéis solares instalados no telhado do estabelecimento comercial **diretamente** para carregar os veículos elétricos, sem exportar para a rede.

- **Descarbonização completa da recarga**: cada kWh solar usado no carregador evita ~0,09 kg de CO₂ (fator de emissão médio da rede elétrica brasileira, ONS 2024);
- **Aproveitamento do excedente solar**: o pico de geração fotovoltaica ocorre entre 10h e 14h, exatamente quando há menor demanda nos estacionamentos — o ChargeGrid armazena esse excedente temporariamente nas baterias dos veículos;
- **Redução do custo da energia**: o custo marginal da energia solar local (R$ 0,12/kWh) é ~87% menor que a tarifa da rede (R$ 0,95/kWh).

---

## Diagrama de Hardware

```
                    3.3V
                     │
         ┌───────────┤
         │           │
    ┌────┴────┐  ┌───┴──────────────────────┐
    │Potenc.  │  │         ESP32            │
    │(Sensor  │  │                          │
    │ Hall)   │  │  GPIO34 ◄── Potenciômetro│
    │         │  │  GPIO12 ──► Relé (IN)    │
    └────┬────┘  │  GPIO21 ──► LCD SDA      │
         │       │  GPIO22 ──► LCD SCL      │
        GND      │  WiFi  ──► Blynk Cloud   │
                 └──────────────────────────┘
                          │
                    ┌─────▼─────┐
                    │   Relé    │
                    │  Module   │
                    └─────┬─────┘
                          │ NO (Normalmente Aberto)
                    ┌─────▼─────┐
                    │    LED    │ ◄── Simula o
                    │  (Carga)  │     veículo EV
                    └───────────┘
```

---

## Mapa de Pinos

| Pino ESP32 | Componente | Função |
|---|---|---|
| GPIO34 | Potenciômetro | Leitura analógica (ADC) — corrente simulada |
| GPIO12 | Módulo Relé | Saída digital — liga/desliga carregamento |
| GPIO21 | LCD SDA | I²C Data |
| GPIO22 | LCD SCL | I²C Clock |

---

## Configuração do Blynk IoT

### Virtual Pins necessários no Template

| Pin | Tipo | Widget | Descrição |
|---|---|---|---|
| V0 | Read | Gauge | Corrente (0–40A) |
| V1 | Write | Switch | Autorizar/Cancelar sessão |
| V2 | Read | Gauge | Potência (0–10.000W) |
| V3 | Read | Gauge | Energia acumulada (kWh) |
| V4 | Read | Value Display | Custo da sessão (R$) |
| V5 | Read | LED | Peak Shaving ativo |
| V6 | Write | Switch | Modo PV-to-EV |

---

## Como Executar

### Wokwi (Simulação)

1. Acesse [wokwi.com](https://wokwi.com) e crie um novo projeto ESP32
2. Adicione os componentes: Potenciômetro, Relé, LCD I²C 16x2
3. Cole o código `chargegrid_esp32.ino` no editor
4. Instale as bibliotecas no `libraries.txt`:
   ```
   BlynkSimpleEsp32
   LiquidCrystal_I2C
   ```
5. Configure seu Template ID e Auth Token do Blynk IoT
6. Inicie a simulação — o ESP32 conectará automaticamente via `Wokwi-GUEST`

### Blynk IoT

1. Crie uma conta em [blynk.cloud](https://blynk.cloud)
2. Crie um novo Template com os virtual pins da tabela acima
3. Crie um dispositivo e copie o Auth Token
4. Monte o dashboard com os widgets indicados
5. Substitua o `BLYNK_AUTH_TOKEN` no código pelo seu token

---

## Dados Gerados pelo Sistema

Exemplo de uma sessão de carregamento simulada:

| t (s) | Corrente (A) | Potência (W) | Peak Shaving | Energia (kWh) | Custo (R$) |
|---|---|---|---|---|---|
| 0 | 0,0 | 0 | ❌ | 0,000 | 0,00 |
| 10 | 18,5 | 4.255 | ❌ | 0,012 | 0,01 |
| 30 | 27,3 | 6.279 | ✅ → 16A | 0,036 | 0,03 |
| 60 | 16,0 | 3.680 | ✅ | 0,061 | 0,06 |
| 120 | 16,0 | 3.680 | ✅ | 0,122 | 0,12 |

> **Modo PV-to-EV ativo**: custo recalculado a R$ 0,12/kWh — economia de ~87% vs tarifa convencional.

---

## Vídeo de Demonstração

🎬 **[Link do vídeo no YouTube](#)** *(será atualizado após gravação)*

---

*FIAP — Faculdade de Informática e Administração Paulista | 2026*  
*Turma 1CCPI | Disciplina: Global Solution / EV Challenge*
