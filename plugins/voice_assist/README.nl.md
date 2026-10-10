# Spraakassistent

Praat vanaf het scherm met Assist van Home Assistant. Het scherm luistert zelf naar een wekzin en stuurt pas daarna wat
je zegt naar Home Assistant; het antwoord komt uit de speaker van het scherm.

Spraakherkenning, het gesprek en spraak zijn wat je Home Assistant voor Assist gebruikt: zijn eigen gespreksagent met
Whisper en Piper, Home Assistant Cloud, of een AI zoals OpenAI of Claude. Het scherm is een spraaksatelliet in Home
Assistant, in de ruimte die je het geeft.

## Wat je nodig hebt

- Een scherm met een microfoon en een speaker: een bord dat ze heeft (de reTerminal D1001) of een plugin die ze brengt.
- Een Assist-pijplijn in Home Assistant (Instellingen, Spraakassistenten) met spraakherkenning en spraak.

## Instellen

1. Open in Tessera **Plugins**, kies **Spraakassistent** en vink je schermen aan. Elk scherm bouwt één keer.
2. Open in Home Assistant het apparaat van het scherm (Instellingen, Apparaten) en kies bij **Assist-pijplijn** de
   pijplijn, als het niet de voorkeurspijplijn is.
3. Zet de tegel **Assistent** op een pagina: hij zegt wat de assistent doet, en een tik stelt een vraag zonder wekzin,
   of stopt er een.
4. Optioneel: zet **Assistent** in de bovenbalk van een pagina (Bovenbalk, Toevoegen, Uit plugins). Hij verschijnt
   terwijl de assistent luistert, nadenkt of antwoordt.

## Op het scherm

Houd de bovenbalk vast, dan **Instellingen**, **Plugins**, **Spraakassistent**:

- **Wekwoord**: of het scherm naar zijn wekzin luistert. Uit stelt een tik op de tegel nog steeds een vraag.
- **Wekzin**: Okay Nabu, Hey Jarvis of Hey Mycroft.
- **Nu vragen**: stelt meteen een vraag.

Beide instellingen zijn ook entiteiten van het scherm in Home Assistant ("Wake word", "Wake phrase"), en de editor
toont ze in de details van de plugin op het tabblad Plugins van het scherm. De eigen schakelaar **Microfoon** van het
scherm, op een bord dat er een heeft, zet hem ook voor de assistent uit.

## Privacy

De wekzin wordt op het scherm zelf herkend; er verlaat niets het scherm voordat hij gehoord is of je op de tegel tikt.
Daarna gaat de vraag naar je Home Assistant en verder naar de spraak- en gespreksdiensten die zijn pijplijn gebruikt.
