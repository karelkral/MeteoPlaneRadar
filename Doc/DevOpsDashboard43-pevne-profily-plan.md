# DevOpsDashboard43 – pevné profily REST API (plán)

Stav: **implementováno** (firmware 0.2.0, kroky 1–6 po jednotlivých commitech); ověření na zařízení
(krok 7) zbývá. Inspirace: MeteoPlaneRadar má adresy všech
služeb jako konstanty ve firmwaru (`Config.h`, `ADSB.h`, `CHMU.h`, …); v NVS
jsou jen uživatelské volby.

## Cíl

Zjednodušit uložení adres REST API. Profily připojení zůstanou, ale budou
pevně pojmenované a jejich URL budou zapsané v programu. V konfiguraci zůstane
jen **aktivní profil** a **API klíč každého profilu**.

| Profil | Base URL |
| --- | --- |
| DeveProduction | `https://tfsstage.devops.scia.cz/deveproduction` |
| DeveStaging | `https://tfsstage.devops.scia.cz/devestaging` |
| Production | `https://tfs.devops.scia.cz/Production` |

Firmware dál připojí `API_PATH_POIROT` (`/IotDashboard/PoirotComputers`) a klíč
posílá v hlavičce `X-Api-Key`.

## Rozhodnutí

| Otázka | Rozhodnutí |
| --- | --- |
| Kde budou URL | přímo v [Config.h](../DevOpsDashboard43/Config.h) (repozitář je veřejný, adresy budou vidět na GitHubu) |
| Stávající klíče v NVS | smazat, zadají se znovu |
| Protokol | pouze `https://`, kontrola při kompilaci |
| Heslo správce pro změnu klíče / aktivního profilu | ne (beze změny proti dnešku) |
| Mazání uloženého klíče | zaškrtávátko „smazat klíč“ u profilu (prázdné pole klíč ponechá) |
| Výchozí aktivní profil (po smazání starého nastavení i po továrním resetu) | **Production** |

## Současný stav

- Tři volné profily (název, base URL, klíč) v NVS: `apN0..2`, `apU0..2`,
  `apK0..2`, aktivní `apAct`; migrace jediného `apiUrl` z 0.1.0
  ([Settings.cpp](../DevOpsDashboard43/Settings.cpp)).
- Web ([WebPage.h](../DevOpsDashboard43/WebPage.h)) umožňuje měnit název, URL
  i klíč; povoleno `http://` i `https://`.
- `POST /api/config` ([WebConfig.cpp](../DevOpsDashboard43/WebConfig.cpp))
  nevyžaduje heslo (heslo chrání jen svou změnu, OTA a reset). Prázdný klíč
  znamená „ponechat uložený“.
- TLS ověřuje certifikát proti balíku CA z core (`NET_TLS_INSECURE 0`).

## Bezpečnost

**Přínos – odpadá odklonění klíče na cizí server.** Dnes může kdokoli v síti
bez hesla změnit `baseUrl` profilu na vlastní server (klíč se přitom zachová)
a zařízení mu při dalším stažení nebo testu pošle uložený `X-Api-Key` – i přes
nešifrované `http://`. S pevnými HTTPS adresami a ověřeným certifikátem tato
cesta zmizí.

**Riziko – zveřejnění adres.** Repozitář `karelkral/MeteoPlaneRadar` je
veřejný; adresy prozradí názvy serverů a prostředí. Samy nejsou tajné, ale
usnadňují průzkum infrastruktury. (Alternativa, nyní nezvolená: lokální
hlavička v `.gitignore` + commitnutý vzor `.example`.)

**Beze změny:** klíče jsou v NVS v čitelné podobě (jako heslo WiFi); kdo přečte
flash, přečte je. URL budou čitelné i v `.bin`.

## Nevýhody pevných URL

1. Změna adresy = nový build a OTA.
2. Nelze rychle vyzkoušet jiný server (např. lokální vývojový).
3. Profil nejde přejmenovat ani přidat bez zásahu do kódu.

## Kroky implementace

1. **[Config.h](../DevOpsDashboard43/Config.h)** – tabulka
   `API_PROFILE_LIST` se třemi dvojicemi `{název, URL}`, upravit komentář sekce
   DevOps REST API. `API_PROFILE_NAME_MAX` a `API_URL_MAX` zůstanou (velikosti
   bufferů v `DevOpsApi` a kontrola délek). Nová konstanta
   `API_PROFILE_DEFAULT 2` (Production).
2. **[Settings.h](../DevOpsDashboard43/Settings.h) /
   [Settings.cpp](../DevOpsDashboard43/Settings.cpp)**
   - `ApiProfile`: `name` a `baseUrl` jako `const char*` z tabulky, v RAM/NVS
     jen `key`.
   - `static_assert`: počet položek = `API_PROFILES`; URL začíná `https://`,
     nekončí `/`, vejde se do `API_URL_MAX`; název 1..`API_PROFILE_NAME_MAX`;
     `API_PROFILE_DEFAULT < API_PROFILES`.
   - Aktivní profil: výchozí `API_PROFILE_DEFAULT` při chybějícím `apiProf`,
     neplatné hodnotě i v `Settings_ClearAll()`.
   - `Settings_SetApiProfile()` nahradit `Settings_SetApiKey(slot, key)`
     (ořez mezer, kontrola `API_KEY_MAX`).
   - Odstranit `normalizeBaseUrl()` a migraci `apiUrl`.
   - Nové NVS klíče `akey0..2` a `apiProf`; při startu smazat staré
     (`apN*`, `apU*`, `apK*`, `apAct`, `apiUrl`) a zalogovat výzvu k novému
     zadání klíčů.
   - `Settings_ClearAll()` obnoví profily z tabulky.
3. **[DevOpsApi.cpp](../DevOpsDashboard43/DevOpsApi.cpp)** – jen drobnosti:
   `baseUrl` už nebude prázdné (`API_OFF` zůstane jako pojistka), z testu
   spojení odpadne text „no URL“.
4. **[WebConfig.cpp](../DevOpsDashboard43/WebConfig.cpp)** – `POST /api/config`:
   `apiProfiles[i]` přijme `key` (neprázdný = uložit) a `clearKey: true`
   (smazat); `name` a `baseUrl` ignoruje. Chybějící nebo prázdný `key` bez
   `clearKey` klíč ponechá. `GET` dál vrací `name`, `baseUrl`, `hasKey` (jen
   ke čtení).
5. **[WebPage.h](../DevOpsDashboard43/WebPage.h)** – u profilu název a URL jako
   text, pole pro klíč, zaškrtávátko „smazat klíč“ (jen když je klíč uložen),
   tlačítko Test; upravit nápovědu a chybové hlášky.
6. **Dokumentace**
   - [README.md](../DevOpsDashboard43/README.md): sekce DevOps API a
     `/api/config`; změnit větu, že skutečné adresy do repozitáře nepatří.
   - [CHANGELOG.md](../DevOpsDashboard43/CHANGELOG.md): upravit položky 0.2.0
     (zatím nevydaná, verze se nezvyšuje).
   - [DevOpsDashboard43-REST-API-plan.md](DevOpsDashboard43-REST-API-plan.md)
     a [programátorská dokumentace](DevOpsDashboard43-programatorska-dokumentace.md).
7. **Ověření** – build přes `build.cmd`; na zařízení: zadání klíče, smazání
   klíče zaškrtávátkem, přepnutí profilu, Test, po startu smazané staré klíče
   a aktivní Production, totéž po továrním resetu; klíč se neobjeví v
   `/api/config`, `/api/status` ani v sériovém logu.

## Otevřené otázky

Žádné – připraveno k implementaci.
