<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru_RU">
<context>
    <name>GoogleSettingsDialog</name>
    <message>
        <location filename="../src/UI/googlesettingsdialog.cpp" line="30"/>
        <source>Source language</source>
        <translation>Исходный язык</translation>
    </message>
    <message>
        <location filename="../src/UI/googlesettingsdialog.cpp" line="34"/>
        <source>Target language</source>
        <translation>Целевой язык</translation>
    </message>
</context>
<context>
    <name>HookController</name>
    <message>
        <location filename="../src/controllers/hookcontroller.cpp" line="96"/>
        <source>[Hook] No process selected. Please choose a process in the settings</source>
        <translation>[Hook] Процесс не выбран. Пожалуйста, выберите процесс в настройках</translation>
    </message>
    <message>
        <location filename="../src/controllers/hookcontroller.cpp" line="112"/>
        <source>[Hook] No game selected. Please choose a game in the settings</source>
        <translation>[Hook] Не выбрана игра. Пожалуйста, выберите игру в настройках</translation>
    </message>
</context>
<context>
    <name>HookSelectorDialog</name>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="107"/>
        <source>Hook text selection</source>
        <translation>Выбор текста хука</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="155"/>
        <source>Show only the block that changed last</source>
        <translation>Показывать только блок, изменившийся последним</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="156"/>
        <source>Show all ticked blocks together</source>
        <translation>Показывать все отмеченные блоки вместе</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="165"/>
        <source>Wait for other blocks:</source>
        <translation>Ждать другие блоки:</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="169"/>
        <source> ms</source>
        <translation> мс</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="171"/>
        <source>When two blocks change at almost the same time, wait this long after the first one before deciding which to show, then show the higher-priority block (set the order with the ▲ ▼ arrows). Increase it if a lower block still flashes before your preferred one; 0 shows whatever changes first.</source>
        <translation>Если два блока меняются почти одновременно, подождать указанное время после первого, прежде чем решить, какой показать, а затем показать блок с более высоким приоритетом (порядок задается стрелками ▲ ▼). Увеличьте значение, если блок с более низким приоритетом все еще мелькает перед нужным; 0 - показывать тот, что изменился первым.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="259"/>
        <source>Apply</source>
        <translation>Применить</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="273"/>
        <source>Select at least one specific block to save - an address (Hook:0x...), a variant (Hook:v1, Hook:v2, ...), a named source (Hook:Textbox, ...) or a group. The plain &quot;Hook&quot; stream cannot be saved.</source>
        <translation>Выберите хотя бы один конкретный блок для сохранения - адрес (Hook:0x...), вариант (Hook:v1, Hook:v2, ...), именованный источник (Hook:Textbox, ...) или группу. Обычный поток «Hook» сохранить нельзя.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="293"/>
        <source>Hook addresses are runtime memory pointers. In some games they are stable and will be restored on the next launch; in others they change every run and won&apos;t match.

Save anyway?</source>
        <translation>Адреса хуков - это указатели в памяти времени выполнения. В одних играх они стабильны и восстанавливаются при следующем запуске; в других меняются при каждом запуске и не совпадут.

Все равно сохранить?</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="207"/>
        <source>Saved addresses (auto-applied when they reappear):</source>
        <translation>Сохраненные адреса (применяются автоматически при повторном появлении):</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="87"/>
        <source>  [variant %1]</source>
        <translation>  [вариант %1]</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="135"/>
        <source>Some games show several text blocks at once (for example a choice menu). Tick a block&apos;s checkbox to show it in the overlay. Click a row to pick it for grouping (highlighted), then press Group to translate the picked blocks as one.</source>
        <translation>Некоторые игры показывают сразу несколько текстовых блоков (например, меню выбора). Отметьте флажок блока, чтобы показать его в оверлее. Щелкните по строке, чтобы выбрать ее для группировки (подсветится), затем нажмите «Группировать», чтобы перевести выбранные блоки как один.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="146"/>
        <source>Search blocks by address or text…</source>
        <translation>Поиск блоков по адресу или тексту…</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="236"/>
        <location filename="../src/UI/hookselectordialog.cpp" line="344"/>
        <source>Group</source>
        <translation>Группировать</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="237"/>
        <source>Combine the selected blocks into one: their texts are joined and translated together, shown as a single block. Select at least two first.</source>
        <translation>Объединить выбранные блоки в один: их тексты соединяются и переводятся вместе, отображаясь как единый блок. Сначала выберите хотя бы два.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="248"/>
        <source>Group blocks</source>
        <translation>Сгруппировать блоки</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="260"/>
        <source>Show the current text for the selected blocks now. Without this a block you just selected appears only on its next update.</source>
        <translation>Показать текущий текст выбранных блоков сейчас. Без этого только что выбранный блок появится только при следующем обновлении.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="266"/>
        <source>Save</source>
        <translation>Сохранить</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="305"/>
        <source>Clear</source>
        <translation>Очистить</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="307"/>
        <source>Forget all hook text seen so far and empty the overlay. Saved addresses are kept. Useful after a per-character variant flooded the list.</source>
        <translation>Забыть весь показанный до сих пор текст хука и очистить оверлей. Сохраненные адреса сохраняются. Полезно, если список переполнил вариант, создаваемый для каждого символа.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="344"/>
        <location filename="../src/UI/hookselectordialog.cpp" line="396"/>
        <location filename="../src/UI/hookselectordialog.cpp" line="521"/>
        <source>Group (%1)</source>
        <translation>Группа (%1)</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="448"/>
        <source>v%1</source>
        <translation>v%1</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="546"/>
        <source>Forget this saved group.</source>
        <translation>Забыть эту сохраненную группу.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="249"/>
        <source>Click at least two blocks or groups to highlight and combine them.</source>
        <translation>Щелкните хотя бы по двум блокам или группам, чтобы подсветить и объединить их.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="267"/>
        <source>Remember the selected blocks so they re-apply next launch.</source>
        <translation>Запомнить выбранные блоки, чтобы применить их снова при следующем запуске.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="272"/>
        <location filename="../src/UI/hookselectordialog.cpp" line="292"/>
        <source>Save selection</source>
        <translation>Сохранить выбор</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="374"/>
        <source>(no hook text yet)</source>
        <translation>(текста хука пока нет)</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="379"/>
        <source>Order blocks with ▲ ▼ - the higher one wins when several change at once, and sets the output order when all are shown. Only saved addresses keep their order next time.</source>
        <translation>Упорядочивайте блоки стрелками ▲ ▼ - верхний побеждает, когда несколько меняются одновременно, и задает порядок вывода, когда показаны все. Порядок до следующего раза сохраняется только у сохраненных адресов.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="418"/>
        <source>Show this block in the overlay (press Apply to take effect).</source>
        <translation>Показывать этот блок в оверлее (нажмите «Применить», чтобы изменения вступили в силу).</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="466"/>
        <source>Higher priority</source>
        <translation>Повысить приоритет</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="467"/>
        <source>Lower priority</source>
        <translation>Понизить приоритет</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="438"/>
        <source>Ungroup: split this group back into separate blocks.</source>
        <translation>Разгруппировать: разбить эту группу обратно на отдельные блоки.</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="497"/>
        <source>(nothing saved)</source>
        <translation>(ничего не сохранено)</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="507"/>
        <location filename="../src/UI/hookselectordialog.cpp" line="545"/>
        <source>Delete</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <location filename="../src/UI/hookselectordialog.cpp" line="508"/>
        <source>Forget this saved address.</source>
        <translation>Забыть этот сохраненный адрес.</translation>
    </message>
</context>
<context>
    <name>HookSettingsDialog</name>
    <message>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="48"/>
        <source>Game / Application</source>
        <translation>Игра / Приложение</translation>
    </message>
    <message>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="49"/>
        <source>Engine</source>
        <translation>Движок</translation>
    </message>
    <message>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="63"/>
        <source>— Select game/app —</source>
        <translation>— Выберите игру/приложение —</translation>
    </message>
    <message>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="74"/>
        <source>Target:</source>
        <translation>Цель:</translation>
    </message>
    <message>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="80"/>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="197"/>
        <source>— Select engine —</source>
        <translation>— Выберите движок —</translation>
    </message>
    <message>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="92"/>
        <source>Engine:</source>
        <translation>Движок:</translation>
    </message>
    <message>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="94"/>
        <source>Process</source>
        <translation>Процесс</translation>
    </message>
    <message>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="97"/>
        <source>Filter by PID or process name...</source>
        <translation>Фильтр по PID или имени процесса...</translation>
    </message>
    <message>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="107"/>
        <source>Process name</source>
        <translation>Имя процесса</translation>
    </message>
    <message>
        <location filename="../src/UI/hooksettingsdialog.cpp" line="100"/>
        <source>Refresh</source>
        <translation>Обновить</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="20"/>
        <source>Settings</source>
        <translation>Настройки</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="44"/>
        <location filename="../src/UI/forms/mainwindow.ui" line="104"/>
        <location filename="../src/UI/forms/mainwindow.ui" line="416"/>
        <location filename="../src/UI/forms/mainwindow.ui" line="1460"/>
        <source>General</source>
        <translation>Общие</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="49"/>
        <source>Output</source>
        <translation>Вывод</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="54"/>
        <source>Translator</source>
        <translation>Переводчик</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="59"/>
        <source>Text processing</source>
        <translation>Обработка текста</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="74"/>
        <source>Proxy</source>
        <translation>Прокси</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="79"/>
        <source>Logs</source>
        <translation>Логи</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="84"/>
        <location filename="../src/UI/forms/mainwindow.ui" line="1590"/>
        <source>About</source>
        <translation>О программе</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="110"/>
        <source>Language</source>
        <translation>Язык</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="133"/>
        <source>Hide window on startup</source>
        <translation>Скрыть окно при запуске</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="167"/>
        <source>Global shortcuts</source>
        <translation>Глобальные горячие клавиши</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="176"/>
        <location filename="../src/UI/forms/mainwindow.ui" line="1513"/>
        <source>Type</source>
        <translation>Тип</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="265"/>
        <source>[Portal] Open hotkey binding</source>
        <translation>[Portal] Открыть привязку горячих клавиш</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="233"/>
        <source>Show/Hide translation history</source>
        <translation>Показать/Скрыть историю перевода</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="285"/>
        <source>Manual Translate</source>
        <translation>Ручной перевод</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="331"/>
        <source>Screencast</source>
        <translation>Трансляция экрана</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="343"/>
        <source>Original screencast</source>
        <translation>Оригинальная трансляция</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="374"/>
        <source>Processed screencast</source>
        <translation>Обработанная трансляция</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="447"/>
        <source>Open selector</source>
        <translation>Открыть переключатель</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="454"/>
        <source>Show original screencast</source>
        <translation>Отображать оригинальную трансляцию</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="464"/>
        <source>Show processed screencast</source>
        <translation>Отображать обработанную трансляцию</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="474"/>
        <source>Frame rate</source>
        <translation>Частота кадров</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="531"/>
        <source>Enable blur</source>
        <translation>Включить размытие</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="524"/>
        <source>Threshold method</source>
        <translation>Метод порогового значения</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="540"/>
        <source>Simple</source>
        <translation>Простой</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="550"/>
        <source>Adaptive</source>
        <translation>Адаптивный</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="728"/>
        <source>Blur settings</source>
        <translation>Настройки размытия</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="808"/>
        <source>Normalize result</source>
        <translation>Нормализовать результат</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="586"/>
        <source>Simple threshold settings</source>
        <translation>Настройки простого порога</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="69"/>
        <source>Configs</source>
        <translation>Конфиги</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="572"/>
        <source>Disable screencast</source>
        <translation>Отключить трансляцию экрана</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="595"/>
        <source>Simple threshold type</source>
        <translation>Тип простого порога</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="631"/>
        <source>Threshold value</source>
        <translation>Пороговое значение</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="657"/>
        <source>Use Otsu&apos;s binarization</source>
        <translation>Использовать бинаризацию Оцу</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="681"/>
        <source>Adaptive threshold settings</source>
        <translation>Настройки адаптивного порога</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="690"/>
        <source>Adaptive method</source>
        <translation>Адаптивный метод</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="795"/>
        <source>Subtract blurred</source>
        <translation>Вычесть размытие</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="856"/>
        <source>Online</source>
        <translation>Онлайн</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="924"/>
        <source>Offline</source>
        <translation>Автономный</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="992"/>
        <source>Input source</source>
        <translation>Источник ввода</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1011"/>
        <source>OCR</source>
        <translation>OCR</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1175"/>
        <source>Clipboard</source>
        <translation>Буфер обмена</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1205"/>
        <source>Text filters</source>
        <translation>Текстовые фильтры</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1218"/>
        <source>Add</source>
        <translation>Добавить</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1229"/>
        <source>String replacement</source>
        <translation>Замена строки</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1211"/>
        <source>Remove</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1296"/>
        <source>Original content</source>
        <translation>Исходное содержание</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1301"/>
        <source>Replace with</source>
        <translation>Заменить на</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1330"/>
        <source>Name</source>
        <translation>Имя</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1340"/>
        <source>Architecture</source>
        <translation>Архитектура</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1350"/>
        <source>Description</source>
        <translation>Описание</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1358"/>
        <source>Open plugins directory</source>
        <translation>Открыть папку с плагинами</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1365"/>
        <source>Reload</source>
        <translation>Обновить</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1335"/>
        <source>Version</source>
        <translation>Версия</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1250"/>
        <source>Apply rules:</source>
        <translation>Применять правила:</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1286"/>
        <source>Regex</source>
        <translation>Регулярное выражение</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1291"/>
        <source>Source</source>
        <translation>Источник</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1345"/>
        <source>Dependencies</source>
        <translation>Зависимости</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1381"/>
        <source>Profiles</source>
        <translation>Профили</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1387"/>
        <source>Active profile: Default</source>
        <translation>Активный профиль: Default</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1394"/>
        <source>Profiles store everything except the Common and Proxy tabs. Save the current settings as a profile, then load it from here whenever you need it.</source>
        <translation>Профили хранят все настройки, кроме вкладок «Общие» и «Прокси». Сохраните текущие настройки как профиль и загружайте его отсюда, когда понадобится.</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1409"/>
        <source>New from current…</source>
        <translation>Новый из текущего…</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1419"/>
        <source>Load</source>
        <translation>Загрузить</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1429"/>
        <source>Rename…</source>
        <translation>Переименовать…</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1439"/>
        <source>Delete</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1466"/>
        <source>Use proxy</source>
        <translation>Использовать прокси</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1483"/>
        <source>Port</source>
        <translation>Порт</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1493"/>
        <source>User</source>
        <translation>Пользователь</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1503"/>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1561"/>
        <source>Copy all</source>
        <translation>Скопировать все</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1568"/>
        <source>Open logs directory</source>
        <translation>Показать файлы с логами</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1610"/>
        <source>Free and open-source tool for on-screen text translation</source>
        <translation>Свободный и открытый инструмент для перевода текста на экране</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="1669"/>
        <source>License</source>
        <translation>Лицензия</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="223"/>
        <source>Capture OCR region</source>
        <translation>Выделить область OCR</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="64"/>
        <source>Plugins</source>
        <translation>Плагины</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="734"/>
        <source>Blur method</source>
        <translation>Метод размытия</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/mainwindow.ui" line="760"/>
        <source>Blur intensity</source>
        <translation>Сила размытия</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="497"/>
        <source>Open Original Screencast in New Window</source>
        <translation>Открыть оригинальную трансляцию в новом окне</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="500"/>
        <source>Open Processed Screencast in New Window</source>
        <translation>Открыть обработанную трансляцию в новом окне</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="592"/>
        <source>Original Screencast Preview</source>
        <translation>Оригинальная трансляция - предпросмотр</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="597"/>
        <source>Processed Screencast Preview</source>
        <translation>Обработанная трансляция - предпросмотр</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="693"/>
        <source>Active</source>
        <translation>Активен</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="693"/>
        <source>Inactive</source>
        <translation>Неактивен</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1051"/>
        <source>Warning</source>
        <translation>Внимание</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1051"/>
        <source>No screencast selected for OCR</source>
        <translation>Для OCR не выбрана трансляция</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1192"/>
        <source>Everywhere</source>
        <translation>Везде</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1194"/>
        <source>Only in %1</source>
        <translation>Только в %1</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1850"/>
        <source>Active profile: %1</source>
        <translation>Активный профиль: %1</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1855"/>
        <source>%1 (active)</source>
        <translation>%1 (активный)</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1871"/>
        <location filename="../src/UI/mainwindow.cpp" line="1878"/>
        <source>New profile</source>
        <translation>Новый профиль</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1872"/>
        <source>Profile name (snapshots the current settings):</source>
        <translation>Имя профиля (сохранит текущие настройки):</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1879"/>
        <location filename="../src/UI/mainwindow.cpp" line="1924"/>
        <source>A profile named &apos;%1&apos; already exists.</source>
        <translation>Профиль с именем «%1» уже существует.</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1897"/>
        <source>Load profile</source>
        <translation>Загрузка профиля</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1898"/>
        <source>Could not load profile &apos;%1&apos;.</source>
        <translation>Не удалось загрузить профиль «%1».</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1915"/>
        <location filename="../src/UI/mainwindow.cpp" line="1923"/>
        <location filename="../src/UI/mainwindow.cpp" line="1929"/>
        <source>Rename profile</source>
        <translation>Переименование профиля</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1916"/>
        <source>New name:</source>
        <translation>Новое имя:</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1929"/>
        <source>Rename failed.</source>
        <translation>Не удалось переименовать.</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1942"/>
        <location filename="../src/UI/mainwindow.cpp" line="1948"/>
        <source>Delete profile</source>
        <translation>Удаление профиля</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1943"/>
        <source>The active profile can&apos;t be deleted. Load another profile first.</source>
        <translation>Активный профиль нельзя удалить. Сначала загрузите другой профиль.</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1949"/>
        <source>Delete profile &apos;%1&apos;? This cannot be undone.</source>
        <translation>Удалить профиль «%1»? Это действие необратимо.</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1995"/>
        <source>Restart Required</source>
        <translation>Требуется перезапуск</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1996"/>
        <source>Your changes will take effect the next time you start AurexTranslator.</source>
        <translation>Ваши изменения вступят в силу при следующем запуске AurexTranslator.</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1998"/>
        <source>Restart Now</source>
        <translation>Перезапустить сейчас</translation>
    </message>
    <message>
        <location filename="../src/UI/mainwindow.cpp" line="1999"/>
        <source>Later</source>
        <translation>Перезапустить позже</translation>
    </message>
</context>
<context>
    <name>OllamaSettingsDialog</name>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="40"/>
        <source>Model</source>
        <translation>Модель</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="51"/>
        <source>Update list</source>
        <translation>Обновить список</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="59"/>
        <source>Translation Prompt</source>
        <translation>Промпт для перевода</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="61"/>
        <source>Translate the following text from Japanese to English. Return **only the translated text** - no explanations, notes, formatting, or original text. Do not add anything else. Text:</source>
        <translation>Переведи следующий текст с японского на русский. В ответе пришли **только переведенный текст**, - без пояснений, комментариев, форматирования или оригинала. Ничего больше не добавляй. Текст:</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="66"/>
        <source>Vision Prompt</source>
        <translation>Промпт для анализа изображений</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="68"/>
        <source>Analyze the image and tell me what text is shown on it. Then, return **only the extracted text** - no explanations, comments, labels, or extra information. Do not add anything else.</source>
        <translation>Проанализируй изображение и скажи, что на нем написано. Затем пришли **только текст**, который изображен на картинке - без пояснений, комментариев, заголовков или дополнительной информации. Ничего больше не добавляй.</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="73"/>
        <source>Vision Mode</source>
        <translation>Режим анализа изображений</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="74"/>
        <source>Automatic</source>
        <translation>Автоматический</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="75"/>
        <source>Manual</source>
        <translation>Ручной</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="85"/>
        <source>Wait for response</source>
        <translation>Ожидать ответа</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="88"/>
        <source>Auto OCR interval (sec)</source>
        <translation>Интервал авто-распознавания (сек)</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="99"/>
        <source>Warning: Automatic mode heavily utilizes the GPU and may cause high system load. Enabling &quot;Wait for response&quot; reduces GPU load significantly.</source>
        <translation>Внимание: автоматический режим интенсивно использует GPU и может привести к высокой нагрузке на систему. Включение режима «Ожидание ответа» значительно снижает нагрузку на графический процессор.</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="144"/>
        <source>Server Unavailable</source>
        <translation>Сервер недоступен</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="145"/>
        <source>Ollama server is unavailable. Please check if the server is running and the URL is correct.</source>
        <translation>Сервер Ollama недоступен. Проверьте, работает ли сервер и верен ли URL-адрес.</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="153"/>
        <source>No Models Found</source>
        <translation>Модели не найдены</translation>
    </message>
    <message>
        <location filename="../src/UI/ollamasettingsdialog.cpp" line="154"/>
        <source>Failed to load models or the model list is empty. Please check if models are installed on the server.</source>
        <translation>Не удалось загрузить модели, или список моделей пуст. Проверьте, установлены ли модели на сервере.</translation>
    </message>
</context>
<context>
    <name>QHotkey</name>
    <message>
        <location filename="../3rdparty/QHotkey/qhotkey.cpp" line="294"/>
        <source>Failed to register %1. Error: %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../3rdparty/QHotkey/qhotkey.cpp" line="314"/>
        <source>Failed to unregister %1. Error: %2</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../src/main.cpp" line="148"/>
        <source>Open</source>
        <translation>Открыть</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="153"/>
        <source>Exit</source>
        <translation>Закрыть</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="195"/>
        <source>AurexTranslator is already running</source>
        <translation>AurexTranslator уже запущен</translation>
    </message>
</context>
<context>
    <name>ScreenCastWindow</name>
    <message>
        <location filename="../src/UI/forms/screencastwindow.ui" line="20"/>
        <source>Screencast</source>
        <translation>Трансляция экрана</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/screencastwindow.ui" line="54"/>
        <source>Capture source</source>
        <translation>Выбор источника</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/screencastwindow.ui" line="60"/>
        <source>Desktop</source>
        <translation>Рабочий стол</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/screencastwindow.ui" line="70"/>
        <source>Window</source>
        <translation>Окно</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/screencastwindow.ui" line="86"/>
        <source>List</source>
        <translation>Список</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/screencastwindow.ui" line="93"/>
        <source>Update list</source>
        <translation>Обновить список</translation>
    </message>
    <message>
        <location filename="../src/UI/screencastwindow.cpp" line="104"/>
        <source>Settings changed</source>
        <translation>Настройки изменены</translation>
    </message>
    <message>
        <location filename="../src/UI/screencastwindow.cpp" line="105"/>
        <source>There are unsaved changes. Do you want to save them?</source>
        <translation>Есть несохраненные изменения. Хотите ли вы их сохранить?</translation>
    </message>
</context>
<context>
    <name>TesseractSettingsDialog</name>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="51"/>
        <source>Status</source>
        <translation>Статус</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="55"/>
        <source>Language</source>
        <translation>Язык</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="65"/>
        <source>Update list</source>
        <translation>Обновить список</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="73"/>
        <source>Tessdata path</source>
        <translation>Путь к tessdata</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="77"/>
        <source>Browse</source>
        <translation>Обзор</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="90"/>
        <source>Use system tessdata</source>
        <translation>Использовать системный tessdata</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="94"/>
        <source>Processing mode</source>
        <translation>Режим обработки</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="95"/>
        <source>Automatic</source>
        <translation>Автоматический</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="96"/>
        <source>Manual</source>
        <translation>Ручной</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="113"/>
        <source>Auto OCR interval (sec)</source>
        <translation>Интервал авто-распознавания (сек)</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="216"/>
        <source>Invalid Tesseract Data Directory</source>
        <translation>Недопустимая директория Tesseract</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="217"/>
        <source>The specified Tesseract data directory does not exist or is invalid.
Please provide a valid path or try using the system default directory.</source>
        <translation>Указанная директория с данными Tesseract не существует или содержит ошибки.
Проверьте правильность пути к tessdata или используйте системную директорию по умолчанию.</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="229"/>
        <source>No Tesseract available languages found</source>
        <translation>Не найдены языковые данные Tesseract</translation>
    </message>
    <message>
        <location filename="../src/UI/tesseractsettingsdialog.cpp" line="230"/>
        <source>Tesseract could not find any language data in system locations.
Please install Tesseract language packs or specify a custom &apos;tessdata&apos; directory.</source>
        <translation>Tesseract не обнаружил языковые данные в стандартных системных расположениях.
Пожалуйста, установите языковые пакеты Tesseract или укажите пользовательскую директорию &apos;tessdata&apos;.</translation>
    </message>
</context>
<context>
    <name>TextOutputWindow</name>
    <message>
        <location filename="../src/UI/textoutputwindow.h" line="107"/>
        <source>Welcome!</source>
        <translation>Добро пожаловать!</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="48"/>
        <source>Translation history</source>
        <translation>История перевода</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="441"/>
        <source>Wait time before showing accumulated text</source>
        <translation>Задержка перед выводом текста</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="597"/>
        <source>Left</source>
        <translation>По левому краю</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="598"/>
        <source>Right</source>
        <translation>По правому краю</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="599"/>
        <source>Center</source>
        <translation>По центру</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="617"/>
        <source>Show/Hide original text</source>
        <translation>Показать/Скрыть исходный текст</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="620"/>
        <source>Show/Hide translator name</source>
        <translation>Показать/Скрыть имя переводчика</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="624"/>
        <source>Transparency</source>
        <translation>Прозрачность</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="626"/>
        <source>Font size</source>
        <translation>Размер шрифта</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="625"/>
        <source>Font</source>
        <translation>Шрифт</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="213"/>
        <source>Source: %1
Translator: %2
Original:
 %3
Result:
 %4</source>
        <translation>Источник: %1
Переводчик: %2
Оригинальный текст:
 %3
Результат:
 %4</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="438"/>
        <source>Text Output Speed</source>
        <translation>Скорость вывода текста</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation type="obsolete">Удалить</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="614"/>
        <source>Show/Hide source</source>
        <translation>Показать/Скрыть источник</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="627"/>
        <source>Text color</source>
        <translation>Цвет текста</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="628"/>
        <source>Text alignment</source>
        <translation>Выравнивание текста</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="629"/>
        <source>Margin top</source>
        <translation>Верхний отступ</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="630"/>
        <source>Margin bottom</source>
        <translation>Нижний отступ</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="631"/>
        <source>Margin left</source>
        <translation>Левый отступ</translation>
    </message>
    <message>
        <location filename="../src/UI/textoutputwindow.cpp" line="632"/>
        <source>Margin right</source>
        <translation>Правый отступ</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="66"/>
        <source>Create a selection region (OCR)</source>
        <translation>Создать выделенную зону области (OCR)</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="97"/>
        <source>Add an ignore area inside the selection</source>
        <translation>Создать исключающую зону в выделенной области</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="131"/>
        <source>Copy to clipboard</source>
        <translation>Копировать в буфер обмена</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="165"/>
        <source>Show translation history</source>
        <translation>Показать историю перевода</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="199"/>
        <source>Clear all translated texts</source>
        <translation>Очистить все переведенные тексты</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="233"/>
        <source>Retranslate</source>
        <translation>Ручной перевод</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="267"/>
        <source>Manual injection
Use if automatic injection failed</source>
        <translation>Ручная инъекция
Используйте, если автоматическая инъекция не удалась</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="302"/>
        <source>Text output speed</source>
        <translation>Скорость вывода текста</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="336"/>
        <source>Select which hook text to show</source>
        <translation>Выбрать, какой текст хука показывать</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="370"/>
        <source>Open settings</source>
        <translation>Открыть настройки</translation>
    </message>
    <message>
        <location filename="../src/UI/forms/textoutputwindow.ui" line="417"/>
        <source>Exit</source>
        <translation>Закрыть</translation>
    </message>
</context>
</TS>
