# Bluetooth-proxy

> **Let op: de proxy gaat ten koste van hoe vlot het scherm reageert.** Wifi en Bluetooth delen één radio, en het scherm
> vraagt al veel van zijn wifi. Met de proxy aan kunnen tikken en pagina's trager reageren, wordt de wifi zwakker, en
> kunnen live verbindingen zoals camerabeelden en video haperen of wegvallen. Voeg hem alleen toe waar je daarmee kunt
> leven, en zet hem uit met de instelling **Bluetooth-proxy** als het scherm eronder lijdt. Een losse ESP32 als
> Bluetooth-proxy kost een paar euro en neemt niets van het scherm.

Maakt van het scherm een Bluetooth-proxy voor Home Assistant, dezelfde die een kant-en-klare ESPHome Bluetooth-proxy
draait. Home Assistant hoort dan Bluetooth-apparaten bij het scherm (thermometers, plantsensoren, tags voor
aanwezigheid met [Bermuda](https://github.com/agittins/bermuda)) en kan via het scherm met sommige verbinden (een slot,
een lamp), tot drie tegelijk.

## Voordat je hem toevoegt: wifi

Het scherm doet wifi en Bluetooth met één radio, dus elk moment dat het naar Bluetooth luistert is een moment zonder
wifi. De plugin luistert zoals de wifi-proxies van ESPHome zelf, een kort stukje tijd in een vast ritme, maar het scherm
heeft toch minder wifi dan eerst:

- Plaatjes, hoesjes en camerabeelden kunnen langzamer laden. Een tester zag een camerabeeld duidelijk vlotter lopen
  met de proxy uit.
- Bij zwakke wifi kan het scherm trager reageren op Home Assistant, of vaker de verbinding kwijtraken.

Zet de proxy op elk moment uit met de instelling **Bluetooth-proxy** (hieronder), zonder opnieuw bouwen. Toont het
scherm camerabeelden, of hangt het aan de rand van je wifi, dan is een losse ESP32 als Bluetooth-proxy de betere keus:
die kost een paar euro en zet je waar de ontvangst het best is.

## Welke schermen

Elk scherm met een ESP32-P4 (de Guition P4-schermen, het Waveshare P4 86, de M5Stack Tab5, de reTerminal D1001, de
CrowPanel 10.1). De P4 doet Bluetooth via de ESP32-C6 ernaast, die ook de wifi doet, en heeft het geheugen over. De app
biedt de plugin alleen op die schermen aan.

Niet op een ESP32-S3 met een RGB-paneel (de Guition 4-inch, de Waveshare 4,3, 5 en 7-inch en hun soort): Bluetooth heeft
geheugen in de chip nodig dat het paneel en wifi al gebruiken, en zo'n scherm maakt dan geen verbinding meer. Ook niet
op de CYD en andere schermen met 4 MB: daar is geen ruimte voor.

## Wat je nodig hebt

- Een scherm met een ESP32-P4.
- Een recente firmware op de ESP32-C6, de chip naast de P4 die wifi en Bluetooth doet. Veel schermen komen met een
  firmware die te oud is voor Bluetooth (een reTerminal D1001 kwam met 2.3.2). De plugin brengt de update mee, zie "De
  C6 bijwerken" hieronder. Met de oude firmware werkt het scherm zoals altijd en blijft Bluetooth uit.

## Instellen

1. Open in Tessera **Plugins**, kies **Bluetooth-proxy** en vink je scherm aan. Laat **Update van de wifichip**
   aangevinkt.
2. Voeg hem toe. Het scherm bouwt één keer.
3. Open in Home Assistant het apparaat van het scherm. **Bluetooth status** zegt wat de proxy doet. **Luistert**: klaar.
   **Werk de wifichip bij**: de C6-firmware is te oud, volg "De C6 bijwerken" hieronder.
4. In Home Assistant toont **Instellingen**, **Apparaten & diensten**, **Bluetooth** het scherm als proxy, genoemd naar
   het scherm en het Bluetooth-adres van zijn C6 (niet het wifi-adres van het scherm), met de apparaten die het hoort
   zodra Home Assistant ze vindt.

## Op het scherm en in de app

**Bluetooth-proxy** zet Bluetooth aan en uit, en **Bluetooth status** zegt wat hij doet: luistert, uit, start op, of dat
de wifichip zijn update nodig heeft. Beide staan in Home Assistant bij het apparaat van het scherm, en op het scherm zelf:
houd de bovenbalk vast, dan **Instellingen**, **Plugins**, **Bluetooth**. De schakelaar staat ook in de details van de
plugin op het tabblad Plugins van het scherm in Tessera. Uit geeft de radio alleen aan wifi; het scherm houdt hem ook na
een herstart uit tot je hem weer aanzet.

## De C6 bijwerken

De ESP32-C6 heeft een eigen firmware, en ESPHome brengt nieuwere uit. Met **Update van de wifichip** aangevinkt heeft het
apparaat van het scherm in Home Assistant **Wi-Fi co-processor firmware**, met de versie op de C6 en de nieuwste. Hij
installeert nooit vanzelf: een update die misgaat laat de C6 steeds opnieuw starten, en alleen zijn downloadmodus haalt
hem terug, wat bij sommige schermen betekent dat de behuizing open moet. Hij wacht dus op jou:

1. Open in Home Assistant het apparaat van het scherm en **Wi-Fi co-processor firmware**.
2. Druk op **Installeren**, en laat het scherm aan tot hij klaar is (een paar minuten). Het scherm herstart, en
   **Bluetooth status** zegt **Luistert**.

Hij kijkt elke zes uur of er een nieuwere versie is en toont die daar; installeren blijft jouw keus. Wil je hem niet?
Vink **Update van de wifichip** uit in de details van de plugin op het tabblad Plugins van het scherm en kies **Opslaan
en bouwen**.

## Goed om te weten

- Hij voegt ongeveer 370 KB toe aan de firmware, en de update van de wifichip ongeveer 200 KB meer. Elk P4-scherm heeft
  daar ruimte voor.
- De proxy is die van ESPHome zelf: wat Home Assistant ermee doet staat in de
  [ESPHome-documentatie over de Bluetooth-proxy](https://esphome.io/components/bluetooth_proxy/).
- Eerder een Bluetooth-proxy of de C6-update toegevoegd via de **Override YAML** van het scherm? Haal die blokken
  (`esp32_ble_tracker:`, `bluetooth_proxy:`, `http_request:` en de `esp32_hosted`-update) eerst weg, anders vindt de
  build ze twee keer. De update houdt zijn naam, dus Home Assistant houdt zijn entiteit. Zegt die entiteit na het bouwen
  niet beschikbaar, herlaad het scherm dan in Home Assistant (**Instellingen**, **Apparaten & diensten**, **ESPHome**, de
  drie puntjes bij het scherm, **Herladen**).
- Geprobeerd op een reTerminal D1001: zijn C6 ging met Installeren van 2.3.2 naar 2.12.12, en de proxy hoorde in de
  eerste minuut 51 apparaten. Met 0.2.0, door de Tessera-app gebouwd uit de plugin met **Update van de wifichip**, zegt
  het scherm **Bluetooth status** Luistert en de update bijgewerkt.
