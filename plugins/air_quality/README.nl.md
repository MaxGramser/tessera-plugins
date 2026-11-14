# Luchtkwaliteit

Eén tegel die zegt hoe de lucht in een ruimte is. Je kiest de sensoren van de ruimte (een AirGradient ONE, of elke
CO2-, fijnstof-, VOC-, NOx-, temperatuur- en vochtsensor die Home Assistant heeft) en wat voor ruimte het is. De tegel
geeft één oordeel, laat zien welke waarde afwijkt en noemt een eerste stap.

- **De kaart is het oordeel.** Groen zolang alles goed is, geel als een waarde matig is, rood als er een slecht is.
- **Een ring met een boog voor elke sensor.** Een boog is heel zolang zijn waarde ideaal is en wordt korter naarmate
  hij afwijkt. Het getal in het midden is de score van de slechtste waarde, van 100 (ideaal) tot 0.
- **Hoe groter de tegel, hoe meer hij toont.** Eén cel toont het oordeel en de reden. Grotere tegels krijgen een cel
  per sensor met de waarde en een kleine schaal: het streepje is de waarde, het gekleurde deel de zone waarin hij valt.
- **Een tik opent de kaart** met alle waarden. Wijkt er iets af, dan staat er een knop Wat te doen. Die knop, of een tik
  op een waarde, opent de uitleg: waar het vandaan komt, een eerste stap, en de laatste 24 uur.

## Instellen

1. Voeg de plugin toe aan een scherm in de Tessera-app en laat het scherm bijwerken.
2. Zet de tegel Luchtkwaliteit op een pagina.
3. Kies in de instellingen van de tegel de **Soort ruimte**. Die bepaalt wat goed is: een slaapkamer wil koelere,
   frissere lucht dan een werkplaats.
4. Kies de sensoren. Met **Vul vanuit een apparaat** kies je één apparaat (een AirGradient ONE bijvoorbeeld) en elke
   sensor die het heeft wordt ingevuld. Of kies elke sensor zelf: een veld dat je leeg laat telt niet mee. Eén
   CO2-sensor is al genoeg.
5. Als je wilt: onder **Eigen grenzen** stel je in waar "goed" ophoudt voor een waarde. Een leeg veld houdt de grens
   van de ruimte, die het veld toont.

## Hoe hij oordeelt

Elke waarde krijgt een score: 100 is ideaal, bij 80 houdt goed op, bij 60 begint slecht. Het oordeel van de tegel is
dat van de slechtste waarde, zodat één slechte waarde nooit wegvalt tegen vijf goede. De officiële luchtindexen werken
net zo.

Wat goed is, per soort ruimte:

| Ruimte | CO2 goed tot | CO2 slecht vanaf | PM2.5 goed / slecht | Temperatuur goed | Vocht goed |
|---|---|---|---|---|---|
| Woonkamer | 1000 ppm | 1500 ppm | 10 / 25 µg/m³ | 19 tot 24 °C | 40 tot 60 % |
| Slaapkamer | 800 | 1000 | 10 / 25 | 16 tot 20 °C | 40 tot 60 % |
| Kantoor | 800 | 1000 | 10 / 25 | 20 tot 24 °C | 40 tot 60 % |
| Muziekstudio | 800 | 1200 | 10 / 25 | 20 tot 24 °C | 45 tot 55 % |
| Galerie of collectie | 1000 | 1500 | 5 / 10 | 16 tot 25 °C | 40 tot 60 % |
| Werkplaats | 1000 | 1500 | 10 / 25 | 15 tot 25 °C | 35 tot 65 % |
| Babykamer | 800 | 1000 | 10 / 25 | 16 tot 20 °C | 40 tot 60 % |
| Keuken | 1000 | 1500 | 15 / 35 | 18 tot 25 °C | 40 tot 65 % |

- **PM10** is goed tot twee keer de grens van PM2.5.
- **VOC als index** (die van Sensirion, zoals een AirGradient hem geeft) is goed tot 150 en slecht vanaf 250. De index
  is relatief aan de ruimte zelf: 100 is het gemiddelde van de laatste dag, dus hij zegt "meer dan normaal", niet
  hoeveel. Een VOC-sensor in ppb is goed tot 220, een in µg/m³ tot 300.
- **NOx als index** is goed tot 20.
- **Een muziekstudio en een galerie beoordelen ook hoe gelijk het vocht bleef**: hoe ver het in 24 uur bewoog. Een
  studio is goed tot 15 punten en slecht vanaf 30, een galerie goed tot 10 en slecht vanaf 20. Hout en papier lijden
  meer van vocht dat beweegt dan van vocht dat er net naast zit. Het heeft een vochtsensor met statistieken in Home
  Assistant nodig (elke sensor met een state class heeft die).

De getallen volgen wat de Wereldgezondheidsorganisatie (fijnstof), het Duitse Umweltbundesamt (CO2 onder 1000 ppm,
VOC), de WELL-standaard en onderzoek naar slaap en werk (CO2 onder 800 ppm), het Bizot Green Protocol van musea (16 tot
25 °C, 40 tot 60 %, hooguit 10 punten vocht in 24 uur) en de bouwers van gitaren en piano's (45 tot 55 %) zeggen. Waar
ze het oneens zijn, neemt de tegel het midden.

## De tips

De uitleg zegt waar een waarde meestal vandaan komt en wat je eerst kunt proberen: een raam open bij CO2, de afzuigkap
bij fijnstof en NOx, een bevochtiger bij droge lucht. Het zijn vuistregels voor een eerste stap, geschreven voor thuis.
Het is geen medisch of professioneel advies, en de tegel is geen alarm: een rook- of koolmonoxidemelder blijft wat hij
is.

## Wat hij aan Home Assistant vraagt

De tegel leest de sensoren die je kiest. Voor de laatste 24 uur vraagt hij Home Assistant om de statistieken van één
sensor (`recorder/statistics_during_period`): als je een uitleg opent, en elke tien minuten voor het vocht van een
studio of een galerie. Niets verlaat je huis.
