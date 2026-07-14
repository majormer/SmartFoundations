#!/usr/bin/env python3
"""
[#482/#476/#487 release 34.2.0] Best-effort translations for the 16 new localization keys,
emitted as scripts/.local/trans/<lang>.json for apply_translations.py to fill into each
culture's .po. Keyed by msgctxt. {0} placeholders are preserved verbatim (the compile step's
format-pattern gate is the hard check).

One authoritative table (TR[msgctxt][lang]) keeps terminology consistent across the keys.
"""
import json
import os

ROOT = os.path.dirname(__file__)
OUT = os.path.join(ROOT, ".local", "trans")
LANGS = ["de", "es", "fr", "it", "ja", "ko", "pl", "pt-BR", "ru", "tr",
         "zh-Hans", "zh-Hant", "bg", "hu", "no", "uk", "vi", "ar", "fa", "th"]

# msgctxt -> {lang: translation}
TR = {
  # "Daisy-Chain Power:" (asset label, #487)
  "7D7230F74AFBCA6BF4F61B81C23EC15C": {
    "de": "Strom durchschleifen:", "es": "Encadenar energía:", "fr": "Chaînage d'énergie :",
    "it": "Concatena energia:", "ja": "電力の数珠つなぎ:", "ko": "전력 데이지 체인:",
    "pl": "Łańcuch zasilania:", "pt-BR": "Encadear energia:", "ru": "Цепочка питания:",
    "tr": "Gücü zincirle:", "zh-Hans": "电力链式连接：", "zh-Hant": "電力鏈式連接：",
    "bg": "Верижно захранване:", "hu": "Áram láncolása:", "no": "Kjede strøm:",
    "uk": "Ланцюг живлення:", "vi": "Nối chuỗi điện:", "ar": "تسلسل الطاقة:",
    "fa": "زنجیره‌ای کردن برق:", "th": "เชื่อมพลังงานแบบลูกโซ่:",
  },
  # HUD role label "Chain" (#478)
  "SmartFoundations,HUD_RoleChain": {
    "de": "Kette", "es": "Cadena", "fr": "Chaîne", "it": "Catena", "ja": "チェーン",
    "ko": "체인", "pl": "Łańcuch", "pt-BR": "Cadeia", "ru": "Цепь", "tr": "Zincir",
    "zh-Hans": "链", "zh-Hant": "鏈", "bg": "Верига", "hu": "Lánc", "no": "Kjede",
    "uk": "Ланцюг", "vi": "Chuỗi", "ar": "سلسلة", "fa": "زنجیره", "th": "โซ่",
  },
  # HUD role label "Rows" (#478)
  "SmartFoundations,HUD_RoleRows": {
    "de": "Reihen", "es": "Filas", "fr": "Rangées", "it": "File", "ja": "行",
    "ko": "행", "pl": "Rzędy", "pt-BR": "Fileiras", "ru": "Ряды", "tr": "Sıralar",
    "zh-Hans": "行", "zh-Hant": "列", "bg": "Редове", "hu": "Sorok", "no": "Rader",
    "uk": "Ряди", "vi": "Hàng", "ar": "صفوف", "fa": "ردیف‌ها", "th": "แถว",
  },
  # HUD "Power Chain: {0}"  (#487) -- {0} MUST be preserved
  "SmartFoundations,HUD_ScaleDaisyChain": {
    "de": "Stromkette: {0}", "es": "Cadena de energía: {0}", "fr": "Chaîne d'énergie : {0}",
    "it": "Catena di energia: {0}", "ja": "電力チェーン: {0}", "ko": "전력 체인: {0}",
    "pl": "Łańcuch zasilania: {0}", "pt-BR": "Cadeia de energia: {0}", "ru": "Цепь питания: {0}",
    "tr": "Güç zinciri: {0}", "zh-Hans": "电力链：{0}", "zh-Hant": "電力鏈：{0}",
    "bg": "Захранваща верига: {0}", "hu": "Áramlánc: {0}", "no": "Strømkjede: {0}",
    "uk": "Ланцюг живлення: {0}", "vi": "Chuỗi điện: {0}", "ar": "سلسلة الطاقة: {0}",
    "fa": "زنجیره برق: {0}", "th": "โซ่พลังงาน: {0}",
  },
  # "Nearby Logistics Range (m)"  (#476)
  "SmartFoundations,P.NearbyLogisticsRange": {
    "de": "Reichweite nahegelegener Logistik (m)", "es": "Alcance de logística cercana (m)",
    "fr": "Portée de logistique proche (m)", "it": "Raggio logistica vicina (m)",
    "ja": "近接ロジスティクス範囲 (m)", "ko": "인접 물류 범위 (m)",
    "pl": "Zasięg pobliskiej logistyki (m)", "pt-BR": "Alcance de logística próxima (m)",
    "ru": "Радиус ближней логистики (м)", "tr": "Yakın lojistik menzili (m)",
    "zh-Hans": "附近物流范围 (米)", "zh-Hant": "附近物流範圍 (公尺)",
    "bg": "Обхват на близка логистика (м)", "hu": "Közeli logisztika hatótávja (m)",
    "no": "Rekkevidde for nærliggende logistikk (m)", "uk": "Радіус ближньої логістики (м)",
    "vi": "Phạm vi hậu cần lân cận (m)", "ar": "نطاق اللوجستيات القريبة (م)",
    "fa": "برد لجستیک نزدیک (متر)", "th": "ระยะโลจิสติกส์ใกล้เคียง (ม.)",
  },
  # tooltip
  "SmartFoundations,P.NearbyLogisticsRange.TT": {
    "de": "Maximaler Anschluss-zu-Anschluss-Abstand für das automatische Verbinden von Bändern und Rohren nahegelegener Fabriken. Bauplan-Nähte und Verteilerbahnen sind nicht betroffen.",
    "es": "Distancia máxima de conector a conector para la conexión automática de cintas y tuberías de fábricas cercanas. Las uniones de planos y los carriles de colector no se ven afectados.",
    "fr": "Distance maximale connecteur à connecteur pour la connexion automatique des convoyeurs et tuyaux des usines proches. Les jonctions de plan et les voies de collecteur ne sont pas affectées.",
    "it": "Distanza massima connettore-connettore per la connessione automatica di nastri e tubi delle fabbriche vicine. Le giunzioni dei progetti e le corsie dei collettori non sono interessate.",
    "ja": "近くの工場のベルトとパイプの自動接続における、コネクタ間の最大距離。設計図の継ぎ目とマニフォールドのレーンには影響しません。",
    "ko": "인근 공장의 벨트와 파이프 자동 연결에 사용되는 커넥터 간 최대 거리입니다. 청사진 이음새와 매니폴드 레인은 영향을 받지 않습니다.",
    "pl": "Maksymalna odległość między złączami przy automatycznym łączeniu taśm i rur pobliskich fabryk. Nie dotyczy szwów projektów ani torów kolektorów.",
    "pt-BR": "Distância máxima de conector a conector para a conexão automática de esteiras e tubos de fábricas próximas. Junções de projetos e trilhas de coletor não são afetadas.",
    "ru": "Максимальное расстояние между разъёмами для автоподключения лент и труб ближних фабрик. Стыки чертежей и линии коллекторов не затрагиваются.",
    "tr": "Yakındaki fabrikaların bant ve boru otomatik bağlantısı için maksimum bağlayıcıdan bağlayıcıya mesafe. Taslak dikişleri ve manifold hatları etkilenmez.",
    "zh-Hans": "附近工厂传送带和管道自动连接的接口间最大距离。蓝图接缝和汇流排车道不受影响。",
    "zh-Hant": "附近工廠輸送帶與管道自動連接的接口間最大距離。藍圖接縫與匯流排車道不受影響。",
    "bg": "Максимално разстояние конектор до конектор за автоматично свързване на ленти и тръби на близки фабрики. Шевовете на чертежите и лентите на колекторите не се засягат.",
    "hu": "A közeli gyárak szalagjainak és csöveinek automatikus összekötésénél a csatlakozók közti legnagyobb távolság. A tervrajz-varratokat és az elosztósávokat nem érinti.",
    "no": "Maksimal avstand fra kobling til kobling for automatisk tilkobling av transportbånd og rør i nærliggende fabrikker. Blåkopiskjøter og manifold-baner påvirkes ikke.",
    "uk": "Максимальна відстань між конекторами для автоз'єднання стрічок і труб ближніх фабрик. Шви креслень і лінії колекторів не зачіпаються.",
    "vi": "Khoảng cách tối đa giữa các đầu nối khi tự động kết nối băng chuyền và ống của nhà máy lân cận. Mối nối bản thiết kế và làn gom không bị ảnh hưởng.",
    "ar": "أقصى مسافة من موصل إلى موصل للتوصيل التلقائي لأحزمة وأنابيب المصانع القريبة. لا تتأثر وصلات المخططات ومسارات المجمّع.",
    "fa": "بیشینه فاصله رابط تا رابط برای اتصال خودکار نوارها و لوله‌های کارخانه‌های نزدیک. درزهای نقشه و مسیرهای مانیفولد تأثیری نمی‌پذیرند.",
    "th": "ระยะห่างสูงสุดระหว่างจุดเชื่อมต่อสำหรับการเชื่อมต่ออัตโนมัติของสายพานและท่อของโรงงานใกล้เคียง รอยต่อพิมพ์เขียวและเลนแมนิโฟลด์จะไม่ได้รับผลกระทบ",
  },
  # "Blueprint Seam Auto-Connect"  (#476)
  "SmartFoundations,P.bBlueprintSeamAutoConnectEnabled": {
    "de": "Bauplan-Naht Auto-Verbinden", "es": "Auto-conexión de uniones de plano",
    "fr": "Auto-connexion des jonctions de plan", "it": "Auto-connessione giunzioni progetto",
    "ja": "設計図の継ぎ目の自動接続", "ko": "청사진 이음새 자동 연결",
    "pl": "Auto-łączenie szwów projektu", "pt-BR": "Autoconexão de junções de projeto",
    "ru": "Автосоединение стыков чертежа", "tr": "Taslak dikişi otomatik bağlantısı",
    "zh-Hans": "蓝图接缝自动连接", "zh-Hant": "藍圖接縫自動連接",
    "bg": "Авт. свързване на шевове на чертеж", "hu": "Tervrajz-varrat automatikus összekötése",
    "no": "Auto-kobling av blåkopiskjøter", "uk": "Автоз'єднання швів креслення",
    "vi": "Tự động nối mối bản thiết kế", "ar": "التوصيل التلقائي لوصلات المخطط",
    "fa": "اتصال خودکار درز نقشه", "th": "เชื่อมต่อรอยต่อพิมพ์เขียวอัตโนมัติ",
  },
  "SmartFoundations,P.bBlueprintSeamAutoConnectEnabled.TT": {
    "de": "Verbindet Bänder und Rohre über benachbarte Kopien beim Skalieren eines Bauplans, unabhängig vom Auto-Verbinden nahegelegener Bänder und Rohre.",
    "es": "Conecta cintas y tuberías entre copias adyacentes al escalar un plano, de forma independiente a la auto-conexión de cintas y tuberías cercanas.",
    "fr": "Connecte les convoyeurs et tuyaux entre copies adjacentes lors de la mise à l'échelle d'un plan, indépendamment de l'auto-connexion des convoyeurs et tuyaux proches.",
    "it": "Collega nastri e tubi tra copie adiacenti durante il ridimensionamento di un progetto, indipendentemente dall'auto-connessione di nastri e tubi vicini.",
    "ja": "設計図をスケールする際、隣接するコピー間でベルトとパイプを接続します。近くのベルトとパイプの自動接続とは独立しています。",
    "ko": "청사진을 확장할 때 인접한 복사본 사이의 벨트와 파이프를 연결합니다. 인근 벨트 및 파이프 자동 연결과는 별개로 작동합니다.",
    "pl": "Łączy taśmy i rury między sąsiednimi kopiami podczas skalowania projektu, niezależnie od auto-łączenia pobliskich taśm i rur.",
    "pt-BR": "Conecta esteiras e tubos entre cópias adjacentes ao escalar um projeto, de forma independente da autoconexão de esteiras e tubos próximos.",
    "ru": "Соединяет ленты и трубы между соседними копиями при масштабировании чертежа, независимо от автосоединения ближних лент и труб.",
    "tr": "Bir taslağı ölçeklerken bitişik kopyalar arasında bantları ve boruları bağlar; yakın bant ve boru otomatik bağlantısından bağımsızdır.",
    "zh-Hans": "缩放蓝图时，在相邻副本之间连接传送带和管道，独立于附近的传送带和管道自动连接。",
    "zh-Hant": "縮放藍圖時，在相鄰副本之間連接輸送帶與管道，獨立於附近的輸送帶與管道自動連接。",
    "bg": "Свързва ленти и тръби между съседни копия при мащабиране на чертеж, независимо от авт. свързване на близки ленти и тръби.",
    "hu": "Egy tervrajz méretezésekor összeköti a szalagokat és csöveket a szomszédos másolatok között, a közeli szalag- és csőautomatikus összekötéstől függetlenül.",
    "no": "Kobler transportbånd og rør mellom tilstøtende kopier når en blåkopi skaleres, uavhengig av auto-kobling for nærliggende bånd og rør.",
    "uk": "З'єднує стрічки й труби між сусідніми копіями під час масштабування креслення, незалежно від автоз'єднання ближніх стрічок і труб.",
    "vi": "Kết nối băng chuyền và ống giữa các bản sao liền kề khi phóng to bản thiết kế, độc lập với việc tự động kết nối băng chuyền và ống lân cận.",
    "ar": "يوصّل الأحزمة والأنابيب عبر النسخ المتجاورة عند تحجيم مخطط، بشكل مستقل عن التوصيل التلقائي للأحزمة والأنابيب القريبة.",
    "fa": "هنگام مقیاس‌بندی یک نقشه، نوارها و لوله‌ها را میان نسخه‌های مجاور متصل می‌کند؛ مستقل از اتصال خودکار نوارها و لوله‌های نزدیک.",
    "th": "เชื่อมต่อสายพานและท่อระหว่างสำเนาที่อยู่ติดกันเมื่อปรับขนาดพิมพ์เขียว โดยเป็นอิสระจากการเชื่อมต่อสายพานและท่อใกล้เคียงอัตโนมัติ",
  },
  # "Scale Daisy-Chain Power"  (#487)
  "SmartFoundations,P.bScaleDaisyChainPower": {
    "de": "Strom beim Skalieren durchschleifen", "es": "Encadenar energía al escalar",
    "fr": "Chaîner l'énergie à la mise à l'échelle", "it": "Concatena energia durante il ridimensionamento",
    "ja": "スケール時に電力を数珠つなぎ", "ko": "확장 시 전력 데이지 체인",
    "pl": "Łańcuch zasilania przy skalowaniu", "pt-BR": "Encadear energia ao escalar",
    "ru": "Цепочка питания при масштабировании", "tr": "Ölçeklerken gücü zincirle",
    "zh-Hans": "缩放时链式连接电力", "zh-Hant": "縮放時鏈式連接電力",
    "bg": "Верижно захранване при мащабиране", "hu": "Áram láncolása méretezéskor",
    "no": "Kjede strøm ved skalering", "uk": "Ланцюг живлення під час масштабування",
    "vi": "Nối chuỗi điện khi phóng to", "ar": "تسلسل الطاقة عند التحجيم",
    "fa": "زنجیره‌ای کردن برق هنگام مقیاس‌بندی", "th": "เชื่อมพลังงานแบบลูกโซ่เมื่อปรับขนาด",
  },
  "SmartFoundations,P.bScaleDaisyChainPower.TT": {
    "de": "Wenn verbesserte Stromanschlüsse freigeschaltet sind, verdrahtet das Skalieren von Fabriken und Generatoren benachbarte Gebäude entlang der Raster-X-Achse miteinander.",
    "es": "Cuando se desbloquean los conectores de energía mejorados, escalar fábricas y generadores conecta los edificios adyacentes a lo largo del eje X de la cuadrícula.",
    "fr": "Une fois les connecteurs d'énergie améliorés débloqués, mettre à l'échelle des usines et des générateurs relie les bâtiments adjacents le long de l'axe X de la grille.",
    "it": "Quando i connettori di energia avanzati sono sbloccati, ridimensionare fabbriche e generatori collega gli edifici adiacenti lungo l'asse X della griglia.",
    "ja": "強化型電力コネクタが解除されている場合、工場や発電機をスケールすると、グリッドのX軸に沿って隣接する建物同士が配線されます。",
    "ko": "강화형 전력 커넥터가 해금되면, 공장과 발전기를 확장할 때 그리드 X축을 따라 인접한 건물들이 서로 배선됩니다.",
    "pl": "Po odblokowaniu ulepszonych złączy zasilania skalowanie fabryk i generatorów łączy sąsiednie budynki wzdłuż osi X siatki.",
    "pt-BR": "Quando os conectores de energia aprimorados estão desbloqueados, escalar fábricas e geradores conecta os edifícios adjacentes ao longo do eixo X da grade.",
    "ru": "Когда разблокированы улучшенные разъёмы питания, масштабирование фабрик и генераторов соединяет соседние здания вдоль оси X сетки.",
    "tr": "Geliştirilmiş güç bağlayıcıları açıldığında, fabrikaları ve jeneratörleri ölçeklemek bitişik binaları ızgara X ekseni boyunca birbirine bağlar.",
    "zh-Hans": "解锁强化电力接口后，缩放工厂和发电机会沿网格 X 轴将相邻建筑相互接线。",
    "zh-Hant": "解鎖強化電力接口後，縮放工廠和發電機會沿網格 X 軸將相鄰建築相互接線。",
    "bg": "Когато са отключени надградените захранващи конектори, мащабирането на фабрики и генератори свързва съседните сгради по оста X на мрежата.",
    "hu": "Ha a fejlesztett áramcsatlakozók fel vannak oldva, a gyárak és generátorok méretezése a rács X tengelye mentén összeköti a szomszédos épületeket.",
    "no": "Når oppgraderte strømkoblinger er låst opp, kobler skalering av fabrikker og generatorer tilstøtende bygninger sammen langs rutenettets X-akse.",
    "uk": "Коли розблоковано покращені силові конектори, масштабування фабрик і генераторів з'єднує сусідні будівлі вздовж осі X сітки.",
    "vi": "Khi Đầu nối Điện Nâng cấp được mở khóa, việc phóng to nhà máy và máy phát sẽ nối các công trình liền kề với nhau dọc theo trục X của lưới.",
    "ar": "عند فتح موصّلات الطاقة المُحسّنة، يؤدي تحجيم المصانع والمولّدات إلى ربط المباني المتجاورة معًا على طول محور X للشبكة.",
    "fa": "هنگامی که رابط‌های برق ارتقایافته باز شده باشند، مقیاس‌بندی کارخانه‌ها و مولدها ساختمان‌های مجاور را در امتداد محور X شبکه به هم سیم‌کشی می‌کند.",
    "th": "เมื่อปลดล็อกตัวเชื่อมต่อพลังงานขั้นสูงแล้ว การปรับขนาดโรงงานและเครื่องกำเนิดไฟฟ้าจะเดินสายเชื่อมอาคารที่อยู่ติดกันไปตามแกน X ของกริด",
  },
  # "Tap to Toggle Transform Modes"  (#482)
  "SmartFoundations,P.bToggleTransformModes": {
    "de": "Transformationsmodi per Tippen umschalten", "es": "Alternar modos de transformación al pulsar",
    "fr": "Basculer les modes de transformation par appui", "it": "Attiva modalità di trasformazione con un tocco",
    "ja": "タップで変形モードを切り替え", "ko": "탭하여 변형 모드 전환",
    "pl": "Przełączaj tryby transformacji dotknięciem", "pt-BR": "Alternar modos de transformação com toque",
    "ru": "Переключать режимы преобразования нажатием", "tr": "Dönüşüm modlarını dokunarak değiştir",
    "zh-Hans": "点按切换变换模式", "zh-Hant": "點按切換變換模式",
    "bg": "Превключване на режими на трансформация с докосване", "hu": "Átalakítási módok váltása koppintással",
    "no": "Veksle transformasjonsmoduser med trykk", "uk": "Перемикати режими трансформації дотиком",
    "vi": "Chạm để bật/tắt chế độ biến đổi", "ar": "تبديل أوضاع التحويل بالنقر",
    "fa": "تعویض حالت‌های تبدیل با یک ضربه", "th": "แตะเพื่อสลับโหมดการแปลง",
  },
  "SmartFoundations,P.bToggleTransformModes.TT": {
    "de": "Tippe auf eine Transformationstaste (Abstand, Stufen, Versatz, Rotation oder Rezept bei einer Fabrik), um den Modus einzuschalten; tippe erneut, um ihn auszuschalten. Gedacht für Controller und Steam-Input-Radialmenüs, die keine Taste halten können. Aus behält das normale Halten-Verhalten bei.",
    "es": "Pulsa una tecla de transformación (Espaciado, Escalones, Escalonamiento, Rotación o Receta en una fábrica) para activar el modo; púlsala de nuevo para desactivarlo. Pensado para mandos y menús radiales de Steam Input, que no pueden mantener una tecla. Desactivado conserva el comportamiento normal de mantener pulsado.",
    "fr": "Appuyez sur une touche de transformation (Espacement, Paliers, Décalage, Rotation ou Recette sur une usine) pour activer le mode ; appuyez à nouveau pour le désactiver. Conçu pour les manettes et les menus radiaux de Steam Input, qui ne peuvent pas maintenir une touche. Désactivé conserve le comportement normal de maintien.",
    "it": "Tocca un tasto di trasformazione (Spaziatura, Gradini, Sfalsamento, Rotazione o Ricetta su una fabbrica) per attivare la modalità; toccalo di nuovo per disattivarla. Pensato per controller e menu radiali di Steam Input, che non possono tenere premuto un tasto. Disattivato mantiene il normale comportamento a pressione prolungata.",
    "ja": "変形キー(間隔、段差、ずらし、回転、または工場でのレシピ)をタップするとモードがオンになり、もう一度タップするとオフになります。キーを押し続けられないコントローラーやSteam入力のラジアルメニュー向けです。オフでは通常の長押し動作になります。",
    "ko": "변형 키(간격, 계단, 엇갈림, 회전, 또는 공장의 레시피)를 탭하면 모드가 켜지고, 다시 탭하면 꺼집니다. 키를 누르고 있을 수 없는 컨트롤러와 Steam 입력 방사형 메뉴를 위한 기능입니다. 끄면 일반적인 길게 누르기 동작이 유지됩니다.",
    "pl": "Dotknij klawisza transformacji (Odstęp, Stopnie, Przesunięcie, Obrót lub Receptura na fabryce), aby włączyć tryb; dotknij ponownie, aby go wyłączyć. Przeznaczone dla kontrolerów i menu promienistych Steam Input, które nie mogą przytrzymać klawisza. Wyłączone zachowuje normalne zachowanie przytrzymania.",
    "pt-BR": "Toque em uma tecla de transformação (Espaçamento, Degraus, Escalonamento, Rotação ou Receita em uma fábrica) para ativar o modo; toque novamente para desativá-lo. Feito para controles e menus radiais do Steam Input, que não conseguem manter uma tecla pressionada. Desativado mantém o comportamento normal de segurar.",
    "ru": "Нажмите клавишу преобразования (Интервал, Ступени, Смещение, Поворот или Рецепт на фабрике), чтобы включить режим; нажмите ещё раз, чтобы выключить. Сделано для контроллеров и радиальных меню Steam Input, которые не могут удерживать клавишу. Выключено сохраняет обычное поведение с удержанием.",
    "tr": "Modu açmak için bir dönüşüm tuşuna (Aralık, Basamak, Kaydırma, Döndürme veya bir fabrikada Tarif) dokunun; kapatmak için tekrar dokunun. Bir tuşu basılı tutamayan denetleyiciler ve Steam Input radyal menüleri için tasarlandı. Kapalıyken normal basılı tutma davranışı korunur.",
    "zh-Hans": "点按变换键（间距、台阶、错位、旋转，或工厂上的配方）即可开启该模式；再次点按即可关闭。专为无法长按按键的手柄和 Steam Input 环形菜单设计。关闭时保持正常的长按操作。",
    "zh-Hant": "點按變換鍵（間距、台階、錯位、旋轉，或工廠上的配方）即可開啟該模式；再次點按即可關閉。專為無法長按按鍵的手把和 Steam Input 環形選單設計。關閉時保持正常的長按操作。",
    "bg": "Докоснете клавиш за трансформация (Разстояние, Стъпки, Отместване, Завъртане или Рецепта върху фабрика), за да включите режима; докоснете отново, за да го изключите. Създадено за контролери и радиални менюта на Steam Input, които не могат да задържат клавиш. Изключено запазва нормалното поведение със задържане.",
    "hu": "Koppints egy átalakító billentyűre (Térköz, Lépcsők, Eltolás, Forgatás vagy Recept egy gyárnál) a mód bekapcsolásához; koppints újra a kikapcsolásához. Kontrollerekhez és Steam Input radiális menükhöz készült, amelyek nem tudnak billentyűt nyomva tartani. Kikapcsolva megmarad a szokásos nyomva tartásos működés.",
    "no": "Trykk på en transformasjonstast (Avstand, Trinn, Forskyvning, Rotasjon eller Oppskrift på en fabrikk) for å slå på modusen; trykk igjen for å slå den av. Laget for kontrollere og Steam Input-radialmenyer, som ikke kan holde en tast. Av beholder den vanlige hold-for-å-bruke-oppførselen.",
    "uk": "Торкніться клавіші трансформації (Інтервал, Сходинки, Зсув, Обертання або Рецепт на фабриці), щоб увімкнути режим; торкніться ще раз, щоб вимкнути. Створено для контролерів і радіальних меню Steam Input, які не можуть утримувати клавішу. Вимкнено зберігає звичайну поведінку з утриманням.",
    "vi": "Chạm một phím biến đổi (Khoảng cách, Bậc, So le, Xoay, hoặc Công thức trên nhà máy) để bật chế độ; chạm lại để tắt. Dành cho tay cầm và menu radial của Steam Input, vốn không thể giữ phím. Tắt sẽ giữ hành vi nhấn-giữ thông thường.",
    "ar": "انقر على مفتاح تحويل (التباعد، الدرجات، الإزاحة، التدوير، أو الوصفة على مصنع) لتشغيل الوضع؛ انقر مرة أخرى لإيقافه. مصمّم لوحدات التحكم وقوائم Steam Input الدائرية التي لا يمكنها الاستمرار في الضغط على مفتاح. الإيقاف يبقي سلوك الضغط المستمر المعتاد.",
    "fa": "برای روشن‌کردن حالت، روی یک کلید تبدیل (فاصله، پله‌ها، جابه‌جایی، چرخش، یا دستور روی یک کارخانه) ضربه بزنید؛ برای خاموش‌کردن دوباره ضربه بزنید. برای دسته‌ها و منوهای شعاعی Steam Input ساخته شده که نمی‌توانند کلید را نگه دارند. خاموش، رفتار عادی نگه‌داشتن را حفظ می‌کند.",
    "th": "แตะปุ่มการแปลง (ระยะห่าง, ขั้น, สลับเหลื่อม, การหมุน หรือสูตรบนโรงงาน) เพื่อเปิดโหมด แตะอีกครั้งเพื่อปิด ออกแบบมาสำหรับคอนโทรลเลอร์และเมนูเรเดียลของ Steam Input ซึ่งไม่สามารถกดปุ่มค้างได้ เมื่อปิดจะคงพฤติกรรมกดค้างตามปกติไว้",
  },
  # "Auto-Connect Behavior"  (#476 section)
  "SmartFoundations,Sec.AutoConnectBehavior": {
    "de": "Auto-Verbinden-Verhalten", "es": "Comportamiento de auto-conexión",
    "fr": "Comportement de l'auto-connexion", "it": "Comportamento auto-connessione",
    "ja": "自動接続の挙動", "ko": "자동 연결 동작",
    "pl": "Zachowanie auto-łączenia", "pt-BR": "Comportamento da autoconexão",
    "ru": "Поведение автосоединения", "tr": "Otomatik bağlantı davranışı",
    "zh-Hans": "自动连接行为", "zh-Hant": "自動連接行為",
    "bg": "Поведение на авт. свързване", "hu": "Automatikus összekötés viselkedése",
    "no": "Oppførsel for auto-kobling", "uk": "Поведінка автоз'єднання",
    "vi": "Hành vi tự động kết nối", "ar": "سلوك التوصيل التلقائي",
    "fa": "رفتار اتصال خودکار", "th": "พฤติกรรมการเชื่อมต่ออัตโนมัติ",
  },
  "SmartFoundations,Sec.AutoConnectBehavior.TT": {
    "de": "Steuert das Verhalten, das das Auto-Verbinden nahegelegener Bänder und Rohre teilen.",
    "es": "Controla el comportamiento compartido por la auto-conexión de cintas y tuberías cercanas.",
    "fr": "Contrôle le comportement partagé par l'auto-connexion des convoyeurs et tuyaux proches.",
    "it": "Controlla il comportamento condiviso dall'auto-connessione di nastri e tubi vicini.",
    "ja": "近くのベルトとパイプの自動接続が共有する挙動を制御します。",
    "ko": "인근 벨트와 파이프 자동 연결이 공유하는 동작을 제어합니다.",
    "pl": "Steruje zachowaniem współdzielonym przez auto-łączenie pobliskich taśm i rur.",
    "pt-BR": "Controla o comportamento compartilhado pela autoconexão de esteiras e tubos próximos.",
    "ru": "Управляет поведением, общим для автосоединения ближних лент и труб.",
    "tr": "Yakın bant ve boru otomatik bağlantısının paylaştığı davranışı denetler.",
    "zh-Hans": "控制附近传送带和管道自动连接共用的行为。",
    "zh-Hant": "控制附近輸送帶與管道自動連接共用的行為。",
    "bg": "Управлява поведението, споделяно от авт. свързване на близки ленти и тръби.",
    "hu": "A közeli szalag- és csőautomatikus összekötés által megosztott viselkedést vezérli.",
    "no": "Styrer oppførselen som deles av auto-kobling for nærliggende bånd og rør.",
    "uk": "Керує поведінкою, спільною для автоз'єднання ближніх стрічок і труб.",
    "vi": "Điều khiển hành vi dùng chung bởi việc tự động kết nối băng chuyền và ống lân cận.",
    "ar": "يتحكّم في السلوك المشترك للتوصيل التلقائي للأحزمة والأنابيب القريبة.",
    "fa": "رفتار مشترک اتصال خودکار نوارها و لوله‌های نزدیک را کنترل می‌کند.",
    "th": "ควบคุมพฤติกรรมที่ใช้ร่วมกันโดยการเชื่อมต่อสายพานและท่อใกล้เคียงอัตโนมัติ",
  },
  # "Blueprint Auto-Connect"  (#476 section)
  "SmartFoundations,Sec.BlueprintAutoConnect": {
    "de": "Bauplan Auto-Verbinden", "es": "Auto-conexión de planos",
    "fr": "Auto-connexion de plans", "it": "Auto-connessione progetti",
    "ja": "設計図の自動接続", "ko": "청사진 자동 연결",
    "pl": "Auto-łączenie projektów", "pt-BR": "Autoconexão de projetos",
    "ru": "Автосоединение чертежей", "tr": "Taslak otomatik bağlantısı",
    "zh-Hans": "蓝图自动连接", "zh-Hant": "藍圖自動連接",
    "bg": "Авт. свързване на чертежи", "hu": "Tervrajz automatikus összekötése",
    "no": "Blåkopi auto-kobling", "uk": "Автоз'єднання креслень",
    "vi": "Tự động kết nối bản thiết kế", "ar": "التوصيل التلقائي للمخططات",
    "fa": "اتصال خودکار نقشه", "th": "เชื่อมต่อพิมพ์เขียวอัตโนมัติ",
  },
  "SmartFoundations,Sec.BlueprintAutoConnect.TT": {
    "de": "Steuert die Verbindungen zwischen benachbarten Kopien skalierter Baupläne.",
    "es": "Controla las conexiones entre copias adyacentes de planos escalados.",
    "fr": "Contrôle les connexions entre copies adjacentes de plans mis à l'échelle.",
    "it": "Controlla le connessioni tra copie adiacenti di progetti ridimensionati.",
    "ja": "スケールした設計図の隣接するコピー間の接続を制御します。",
    "ko": "확장된 청사진의 인접한 복사본 사이의 연결을 제어합니다.",
    "pl": "Steruje połączeniami między sąsiednimi kopiami skalowanych projektów.",
    "pt-BR": "Controla as conexões entre cópias adjacentes de projetos escalados.",
    "ru": "Управляет соединениями между соседними копиями масштабированных чертежей.",
    "tr": "Ölçeklenmiş taslakların bitişik kopyaları arasındaki bağlantıları denetler.",
    "zh-Hans": "控制缩放蓝图相邻副本之间的连接。",
    "zh-Hant": "控制縮放藍圖相鄰副本之間的連接。",
    "bg": "Управлява връзките между съседни копия на мащабирани чертежи.",
    "hu": "A méretezett tervrajzok szomszédos másolatai közötti kapcsolatokat vezérli.",
    "no": "Styrer koblingene mellom tilstøtende kopier av skalerte blåkopier.",
    "uk": "Керує з'єднаннями між сусідніми копіями масштабованих креслень.",
    "vi": "Điều khiển các kết nối giữa những bản sao liền kề của bản thiết kế được phóng to.",
    "ar": "يتحكّم في الاتصالات بين النسخ المتجاورة للمخططات المُحجّمة.",
    "fa": "اتصالات میان نسخه‌های مجاور نقشه‌های مقیاس‌بندی‌شده را کنترل می‌کند.",
    "th": "ควบคุมการเชื่อมต่อระหว่างสำเนาที่อยู่ติดกันของพิมพ์เขียวที่ปรับขนาด",
  },
}

# Pre-existing untranslated HUD skip-reason strings (from v33.5.4 auto-connect reporting) - cleaned
# up in this pass so the release ships no English placeholders. {0}/{1} and the leading "*" HUD
# marker are preserved verbatim.
TR.update({
  # "*{0} belt connection(s) skipped: {1}"
  "SmartFoundations,HUD_BeltsSkipped": {
    "de": "*{0} Bandverbindung(en) übersprungen: {1}", "es": "*{0} conexión(es) de cinta omitida(s): {1}",
    "fr": "*{0} connexion(s) de convoyeur ignorée(s) : {1}", "it": "*{0} connessione/i nastro saltata/e: {1}",
    "ja": "*ベルト接続を{0}件スキップ: {1}", "ko": "*벨트 연결 {0}개 건너뜀: {1}",
    "pl": "*Pominięto {0} połączeń taśm: {1}", "pt-BR": "*{0} conexão(ões) de esteira ignorada(s): {1}",
    "ru": "*Пропущено ленточных соединений: {0}: {1}", "tr": "*{0} bant bağlantısı atlandı: {1}",
    "zh-Hans": "*已跳过 {0} 个传送带连接：{1}", "zh-Hant": "*已跳過 {0} 個輸送帶連接：{1}",
    "bg": "*Пропуснати {0} лентови връзки: {1}", "hu": "*{0} szalagkapcsolat kihagyva: {1}",
    "no": "*{0} båndtilkobling(er) hoppet over: {1}", "uk": "*Пропущено стрічкових з'єднань: {0}: {1}",
    "vi": "*Đã bỏ qua {0} kết nối băng chuyền: {1}", "ar": "*تم تخطّي {0} من وصلات الأحزمة: {1}",
    "fa": "*{0} اتصال نوار نادیده گرفته شد: {1}", "th": "*ข้ามการเชื่อมต่อสายพาน {0} จุด: {1}",
  },
  # "*{0} pipe connection(s) skipped: {1}"
  "SmartFoundations,HUD_PipesSkipped": {
    "de": "*{0} Rohrverbindung(en) übersprungen: {1}", "es": "*{0} conexión(es) de tubería omitida(s): {1}",
    "fr": "*{0} connexion(s) de tuyau ignorée(s) : {1}", "it": "*{0} connessione/i tubo saltata/e: {1}",
    "ja": "*パイプ接続を{0}件スキップ: {1}", "ko": "*파이프 연결 {0}개 건너뜀: {1}",
    "pl": "*Pominięto {0} połączeń rur: {1}", "pt-BR": "*{0} conexão(ões) de tubo ignorada(s): {1}",
    "ru": "*Пропущено трубных соединений: {0}: {1}", "tr": "*{0} boru bağlantısı atlandı: {1}",
    "zh-Hans": "*已跳过 {0} 个管道连接：{1}", "zh-Hant": "*已跳過 {0} 個管道連接：{1}",
    "bg": "*Пропуснати {0} тръбни връзки: {1}", "hu": "*{0} csőkapcsolat kihagyva: {1}",
    "no": "*{0} rørtilkobling(er) hoppet over: {1}", "uk": "*Пропущено трубних з'єднань: {0}: {1}",
    "vi": "*Đã bỏ qua {0} kết nối ống: {1}", "ar": "*تم تخطّي {0} من وصلات الأنابيب: {1}",
    "fa": "*{0} اتصال لوله نادیده گرفته شد: {1}", "th": "*ข้ามการเชื่อมต่อท่อ {0} จุด: {1}",
  },
  # "invalid shape ({0})"
  "SmartFoundations,HUD_SkipReason_InvalidShape": {
    "de": "ungültige Form ({0})", "es": "forma no válida ({0})", "fr": "forme invalide ({0})",
    "it": "forma non valida ({0})", "ja": "無効な形状 ({0})", "ko": "잘못된 형태 ({0})",
    "pl": "nieprawidłowy kształt ({0})", "pt-BR": "forma inválida ({0})", "ru": "недопустимая форма ({0})",
    "tr": "geçersiz şekil ({0})", "zh-Hans": "形状无效 ({0})", "zh-Hant": "形狀無效 ({0})",
    "bg": "невалидна форма ({0})", "hu": "érvénytelen alak ({0})", "no": "ugyldig form ({0})",
    "uk": "неприпустима форма ({0})", "vi": "hình dạng không hợp lệ ({0})", "ar": "شكل غير صالح ({0})",
    "fa": "شکل نامعتبر ({0})", "th": "รูปทรงไม่ถูกต้อง ({0})",
  },
  # "lane blocked ({0})"
  "SmartFoundations,HUD_SkipReason_LaneBlocked": {
    "de": "Bahn blockiert ({0})", "es": "carril bloqueado ({0})", "fr": "voie bloquée ({0})",
    "it": "corsia bloccata ({0})", "ja": "レーンが塞がっています ({0})", "ko": "레인 막힘 ({0})",
    "pl": "tor zablokowany ({0})", "pt-BR": "trilha bloqueada ({0})", "ru": "линия заблокирована ({0})",
    "tr": "hat engellendi ({0})", "zh-Hans": "车道受阻 ({0})", "zh-Hant": "車道受阻 ({0})",
    "bg": "лентата е блокирана ({0})", "hu": "sáv elzárva ({0})", "no": "bane blokkert ({0})",
    "uk": "лінію заблоковано ({0})", "vi": "làn bị chặn ({0})", "ar": "المسار محجوب ({0})",
    "fa": "مسیر مسدود است ({0})", "th": "เลนถูกกีดขวาง ({0})",
  },
  # "too close ({0})"
  "SmartFoundations,HUD_SkipReason_TooClose": {
    "de": "zu nah ({0})", "es": "demasiado cerca ({0})", "fr": "trop proche ({0})",
    "it": "troppo vicino ({0})", "ja": "近すぎます ({0})", "ko": "너무 가까움 ({0})",
    "pl": "za blisko ({0})", "pt-BR": "muito perto ({0})", "ru": "слишком близко ({0})",
    "tr": "çok yakın ({0})", "zh-Hans": "太近 ({0})", "zh-Hant": "太近 ({0})",
    "bg": "твърде близо ({0})", "hu": "túl közel ({0})", "no": "for nær ({0})",
    "uk": "надто близько ({0})", "vi": "quá gần ({0})", "ar": "قريب جدًا ({0})",
    "fa": "خیلی نزدیک ({0})", "th": "ใกล้เกินไป ({0})",
  },
  # "too far ({0})"
  "SmartFoundations,HUD_SkipReason_TooFar": {
    "de": "zu weit ({0})", "es": "demasiado lejos ({0})", "fr": "trop loin ({0})",
    "it": "troppo lontano ({0})", "ja": "遠すぎます ({0})", "ko": "너무 멂 ({0})",
    "pl": "za daleko ({0})", "pt-BR": "muito longe ({0})", "ru": "слишком далеко ({0})",
    "tr": "çok uzak ({0})", "zh-Hans": "太远 ({0})", "zh-Hant": "太遠 ({0})",
    "bg": "твърде далеч ({0})", "hu": "túl messze ({0})", "no": "for langt unna ({0})",
    "uk": "надто далеко ({0})", "vi": "quá xa ({0})", "ar": "بعيد جدًا ({0})",
    "fa": "خیلی دور ({0})", "th": "ไกลเกินไป ({0})",
  },
  # "too steep ({0})"
  "SmartFoundations,HUD_SkipReason_TooSteep": {
    "de": "zu steil ({0})", "es": "demasiado empinado ({0})", "fr": "trop raide ({0})",
    "it": "troppo ripido ({0})", "ja": "急すぎます ({0})", "ko": "너무 가파름 ({0})",
    "pl": "zbyt stromo ({0})", "pt-BR": "muito íngreme ({0})", "ru": "слишком круто ({0})",
    "tr": "çok dik ({0})", "zh-Hans": "太陡 ({0})", "zh-Hant": "太陡 ({0})",
    "bg": "твърде стръмно ({0})", "hu": "túl meredek ({0})", "no": "for bratt ({0})",
    "uk": "надто круто ({0})", "vi": "quá dốc ({0})", "ar": "شديد الانحدار ({0})",
    "fa": "خیلی شیب‌دار ({0})", "th": "ชันเกินไป ({0})",
  },
})

os.makedirs(OUT, exist_ok=True)
counts = {}
for lang in LANGS:
    data = {ctx: TR[ctx][lang] for ctx in TR if lang in TR[ctx]}
    with open(os.path.join(OUT, lang + ".json"), "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    counts[lang] = len(data)

# Integrity: every key present for every language, and every {N} placeholder in the English
# source is preserved in every translation (the compile's format-pattern gate is the hard check).
import re as _re
EN = {
  "SmartFoundations,HUD_ScaleDaisyChain": "Power Chain: {0}",
  "SmartFoundations,HUD_BeltsSkipped": "*{0} belt connection(s) skipped: {1}",
  "SmartFoundations,HUD_PipesSkipped": "*{0} pipe connection(s) skipped: {1}",
  "SmartFoundations,HUD_SkipReason_InvalidShape": "invalid shape ({0})",
  "SmartFoundations,HUD_SkipReason_LaneBlocked": "lane blocked ({0})",
  "SmartFoundations,HUD_SkipReason_TooClose": "too close ({0})",
  "SmartFoundations,HUD_SkipReason_TooFar": "too far ({0})",
  "SmartFoundations,HUD_SkipReason_TooSteep": "too steep ({0})",
}
PH = _re.compile(r"\{\d+\}")
missing = {lang: [c for c in TR if lang not in TR[c]] for lang in LANGS}
missing = {k: v for k, v in missing.items() if v}
ph_bad = []
for ctx, src in EN.items():
    want = sorted(set(PH.findall(src)))
    for lang in LANGS:
        got = sorted(set(PH.findall(TR[ctx][lang])))
        if got != want:
            ph_bad.append(f"{ctx}/{lang}: want {want} got {got}")
print("wrote", len(LANGS), "language files, keys each:", set(counts.values()))
print("missing keys:", missing or "none")
print("placeholder mismatches:", ph_bad or "none")
