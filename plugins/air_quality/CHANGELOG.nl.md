# Changelog

## 0.1.2 - 2026-11-26

- Nieuwe grens **VOC goed vanaf**: waar de schaal van een VOC-sensor begint. Daarmee past er naast de Sensirion-index,
  waarvoor de tegel ze leest, ook een VOC-sensor zonder eenheid die vanaf 0 telt bij schone lucht, zoals Amazons Air
  Quality Monitor (0 tot 100, hoger is slechter). Zet hem op 0, en dan **VOC goed tot** in de getallen van die sensor
  zelf. Zolang het veld leeg blijft verandert er niets.

## 0.1.1 - 2026-11-14

- De tegel gaf op dat hij 12 KB in de chip houdt, waardoor een scherm met een paar andere tegels hem weigerde. Hij houdt daar bijna niets: zijn onderdelen staan in het grotere geheugen naast de chip. Een scherm neemt hem nu als elke andere tegel.

## 0.1.0 - 2026-11-14

- De eerste versie. Eén tegel voor de lucht in een ruimte: een oordeel in de kleur van de kaart, een ring met een boog
  per sensor, een cel per waarde op grotere tegels, een kaart met alle waarden, en uitleg per waarde met waar het
  vandaan komt, een eerste stap en de laatste 24 uur. Acht soorten ruimtes, eigen grenzen, en de sensoren van één
  apparaat in één keer ingevuld. Heeft plugin-API 0.26 nodig.
