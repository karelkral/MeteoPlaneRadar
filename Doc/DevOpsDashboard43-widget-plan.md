# DevOps Dashboard 4.3 – plán: vzhled widgetu „Poirot computers“

> Související dokumenty: [plán DevOpsDashboard43](DevOpsDashboard43-plan.md),
> [plán napojení na REST API](DevOpsDashboard43-REST-API-plan.md) (kap. 4.6).
>
> Stav: **plán**, neimplementováno. Plán vznikl 14. 11. 2026.

Dashboard má vypadat jako referenční obrázek: tenký prstencový graf se čtyřmi stavy počítačů,
uprostřed `Error / Total`, vpravo legenda. Rozsah této práce je **jen vzhled na testovacích datech**.
Napojení na API zůstává v [plánu REST API](DevOpsDashboard43-REST-API-plan.md).

## 1. Rozhodnutí

| Oblast | Rozhodnutí |
| --- | --- |
| Rozsah | jen vzhled widgetu na testovacích datech (nový `DashData` se 4 stavy), API až potom |
| Karta | **bez karty**, jen tenký prstenec na černém pozadí |
| Procenta ve výsečích | **ne** |
| Střed prstence | `Error / Total`, např. „1 / 42“, s popiskem „Error / Total“ |
| Nadpis „Poirot computers“ | v liště **místo** „DevOps dashboard“ |
| Legenda | beze změny rozložení: název vlevo, číslo vpravo |
| Barva Waiting | **modrá** (jako v plánu REST API), ne šedá jako na obrázku |

## 2. Stav a rozdíly

[ScreenDashboard.cpp](../DevOpsDashboard43/ScreenDashboard.cpp) už umí prstenec z `lv_arc`
(jedna výseč = jeden oblouk) a legendu. Stačí ho upravit.

| Oblast | Dnes | Nově |
| --- | --- | --- |
| Stavy v [DashData.h](../DevOpsDashboard43/DashData.h) | Succeeded, Failed, Running, Canceled | **Waiting, Running, Warning, Error** a pole `total` |
| Testovací data | `{42, 7, 5, 3}` | `{4, 37, 0, 1}`, total 42 (jako obrázek) |
| Pořadí výsečí | – | po směru hodin od 12 h: Waiting → Running → Warning → Error. Error končí těsně vlevo od vrcholu. |
| Barvy v [Theme.h](../DevOpsDashboard43/Theme.h) | `COL_OK/FAIL/RUN/CANCEL` | `COL_WAITING` modrá (`0x42A5F5`), `COL_RUNNING` zelená, `COL_WARNING` jantarová (`0xD29922`), `COL_ERROR` červená; staré názvy se přejmenují |
| Prstenec | poloměr 115–175 (tlustý), procenta ve výsečích | vnitřní poloměr ~140, vnější 175 (tloušťka ~35 px). Kód procent (`s_pct`, `lv_text_get_size`, goniometrie) se smaže. Mezera 1–2° mezi výsečemi zůstane. |
| Střed | celkový počet + „pipelines / 24h“ | „1 / 42“ + popisek „Error / Total“ (šedě) |
| Nadpis | „DevOps dashboard“ (`TITLES[0]` v [Ui.cpp](../DevOpsDashboard43/Ui.cpp)) | „Poirot computers“ |
| Legenda | kolečko, název, číslo vpravo | stejné rozložení, 4 nové řádky v pořadí Waiting, Running, Warning, Error |
| Bez dat | šedý prstenec a „-“ | šedý prstenec a „- / -“ |

## 3. Technický problém: font

Font `inter_sb_72` obsahuje jen `-` a číslice (`-r 0x2D,0x30-0x39`). Znak `/` (0x2F) v něm chybí,
takže „1 / 42“ by se nevykreslilo.

1. V [gen_fonts.cmd](../DevOpsDashboard43/tools/gen_fonts.cmd) rozšířit rozsah na `-r 0x2D,0x2F,0x30-0x39`
   a font přegenerovat (potřebuje Node.js a internet).
2. `inter_sb_24` (`%` a číslice) po smazání procent nic nepoužívá. Ověřit a odstranit z `Fonts.h`
   i z `gen_fonts.cmd`.
3. Náhradní řešení, kdyby generování selhalo: tři popisky `1`, `/`, `42` vedle sebe.

## 4. Fáze

1. `DashData.h`, `Theme.h`: nové stavy, barvy, testovací data.
2. Font s `/`.
3. `ScreenDashboard.cpp/.h`: tenký prstenec, střed „Error / Total“, 4 řádky legendy, bez procent.
   `Ui.cpp`: titulek.
4. Build přes `build.cmd`, kontrola velikosti firmwaru a paměti. Vizuální kontrola je na desce.
5. Komentáře v hlavičkách, [CHANGELOG.md](../DevOpsDashboard43/CHANGELOG.md).
6. Oprava kap. 4.6 v [plánu REST API](DevOpsDashboard43-REST-API-plan.md): pořadí výsečí, „Error / Total“
   ve středu, nadpis v liště místo popisku pod součtem.

## 5. Rizika

| Riziko | Opatření |
| --- | --- |
| Výseč 1 z 42 má jen ~8,6° | mezera 1° ji nesmí zničit; stávající kód omezuje mezeru na třetinu výseče |
| Chybějící glyf `/` | přegenerovat font, viz kap. 3 |
| Paměť a výkon | počet `lv_arc` se nemění (4 + 1 šedý) |
