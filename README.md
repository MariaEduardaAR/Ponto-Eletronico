# Sistema de Registro de Ponto com ESP32 e RFID

Este projeto implementa um **sistema de ponto eletrônico automatizado** utilizando um microcontrolador **ESP32**, o leitor **RFID MFRC522**, um **display LCD 16x2 via I2C** e integração com o **Google Sheets** através de **requisições HTTP seguras**.

Desenvolvido como parte do projeto acadêmico **"Aplicação de RFID e Eletromagnetismo em um Sistema Automatizado de Registro de Ponto"**.

---

## Funcionalidades

- Leitura contínua de cartões RFID.
- Exibição das mensagens no LCD para entrada, saída, erro ou cartão não cadastrado.
- Registro automático de ponto em uma **planilha Google**.
- Modo cadastro: adicionar novos cartões através de um cartão master.
- Reconexão automática com a rede Wi-Fi.
- Sincronização de horário via **SNTP**.

---

## Tecnologias e Bibliotecas

-  **ESP32** (ESP-IDF framework)
-  **MFRC522 RFID Reader** (SPI)
-  **LCD 16x2** com interface I2C
-  **Google Sheets** + Google Apps Script
-  `esp_http_client` para HTTPS
-  `esp_sntp` para sincronização de horário
-  `cJSON` para parse de respostas JSON

---

## Hardware Utilizado

| Componente         | Função                                 |
|--------------------|----------------------------------------|
| ESP32              | Microcontrolador principal             |
| MFRC522            | Leitor RFID (SPI)                      |
| LCD 16x2           | Feedback visual ao usuário (I2C)       |
| Tags RFID (cartões)| Identificação do usuário               |
| Wi-Fi              | Envio dos dados à nuvem                |

---

## Diagrama de Funcionamento

1. ESP32 conecta ao Wi-Fi.
2. Sincroniza o relógio com um servidor NTP.
3. Leitor MFRC522 escaneia continuamente.
4. Ao detectar um cartão:
   - Se for o **master**, ativa o modo **cadastro**.
   - Se for um cartão comum:
     - Envia o UID via HTTPS para o script do Google.
     - Recebe resposta JSON com nome e status (entrada ou saída).
     - Exibe a informação no LCD.
     - Registra o evento na planilha.


---

## Organização do Código

```

SISTEMADEPONTOELETRONICO-MAIN-ELETRO/
├── main/
│   ├── inc/                      # Arquivos de cabeçalho (headers)
│   │   ├── lcd_i2c.h
│   │   ├── mfrc522.h
│   │   ├── sntp_time.h
│   │   └── wifi_connect.h
│   ├── src/                      # Implementações dos módulos
│   │   ├── lcd_i2c.c
│   │   ├── mfrc522.c
│   │   ├── sntp_time.c
│   │   └── wifi_connect.c
│   ├── main.c                   # Lógica principal do sistema
│   └── CMakeLists.txt
├── .vscode/                     # Configurações do VS Code (opcional)
├── build/                       # Gerado automaticamente (binários)
├── CMakeLists.txt               # Arquivo principal de build do projeto
├── sdkconfig                    # Configurações da SDK ESP-IDF
├── .gitignore                   # Arquivos a serem ignorados pelo Git
└── README.md                    # Documentação do projeto


```

---

## 🧠 Fluxo da `main.c`

- Inicialização de:
  - NVS
  - LCD
  - Wi-Fi
  - SNTP
  - RFID (SPI)
- Criação da task `display_manager_task`
- Registro de eventos RFID com `tag_handler`
- Controle de estados via enum `display_state_t`
- Comunicação com API (Google Apps Script) via `esp_http_client`
- Respostas tratadas com `cJSON`

---

## 🛡️ Segurança e Limitações

- Uso de `esp_crt_bundle_attach` para HTTPS seguro.
- O alcance da leitura RFID é limitado (~3-5 cm).
- Dependente da estabilidade da conexão Wi-Fi.

---

## 📈 Resultados dos Testes

- Leitura confiável em até 4 cm.
- Tempo de resposta: 3-5 segundos.
- Reconexão automática à rede Wi-Fi.
- Evita leituras repetidas do mesmo cartão em curto intervalo (delay de 2 segundos).

---

## Autoria

> **Maria Eduarda Almeida Rodrigues**  
> **Nathalia de Oliveira Lima**  
> Engenharia da Computação – UFC Quixadá  
> Emails: `mariaeduardarodrigues23@alu.ufc.br`, `nathalialima09@alu.ufc.br`

---

## 📌 Licença

Este projeto é de caráter educacional e pode ser usado livremente com atribuição aos autores.

```
