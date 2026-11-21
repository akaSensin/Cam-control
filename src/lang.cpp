/*
	Cam Control - made by sensin
	Localization - see lang.h.

	One row per string, so a string and all of its translations sit together and
	a reworded English line cannot silently shift a table index.
*/

#include "lang.h"
#include "log.h"

#include <windows.h>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace
{
	struct Entry
	{
		const char *key; // English; also the lookup key
		const char *tr;
		const char *es;
		const char *ru;
		const char *ar;
		const char *zh;
		const char *ja;
		const char *de;
		const char *pt;
		const char *id;
	};

	// Every user-visible string in the mod.
	const Entry kStrings[] =
	{
		// ---- main page ------------------------------------------------
		{ "Mod Enabled",
		  "Mod Etkin", "Mod activado", "Мод включён",
		  "المود شغال", "模组已启用", "MOD有効",
		  "Mod aktiviert", "Mod ativado", "Mod Aktif" },
		{ "Mod is OFF - nothing is applied.",
		  "Mod KAPALI - hiçbir şey uygulanmıyor.", "Mod desactivado: no se aplica nada.", "Мод выключен — ничего не применяется.",
		  "المود مقفول - مفيش حاجة متطبقة.", "模组已关闭 - 不应用任何设置。", "MODはオフです - 何も適用されません。",
		  "Mod ist AUS - nichts wird angewendet.", "Mod está DESLIGADO - nada é aplicado.", "Mod NONAKTIF - tidak ada yang diterapkan." },
		{ "Driving the game's own camera. Mouse look stays normal.",
		  "Oyunun kendi kamerası sürülüyor. Fare bakışı normal kalır.", "Controla la cámara del juego. La vista con el ratón sigue normal.", "Управляет камерой самой игры. Обзор мышью остаётся обычным.",
		  "بيتحكم في كاميرا اللعبة نفسها. حركة الماوس بتفضل طبيعية.", "直接驱动游戏自带摄像机，鼠标视角保持正常。", "ゲーム本来のカメラを操作します。マウス視点は通常どおりです。",
		  "Steuert die Spielkamera. Mausblick bleibt normal.", "Controla a câmera do jogo. A visão com o mouse continua normal.", "Menggerakkan kamera asli game. Kontrol mouse tetap normal." },
		{ "Now: %s / %s   (%s)",
		  "Şimdi: %s / %s   (%s)", "Ahora: %s / %s   (%s)", "Сейчас: %s / %s   (%s)",
		  "دلوقتي: %s / %s   (%s)", "当前: %s / %s   (%s)", "現在: %s / %s   (%s)",
		  "Aktuell: %s / %s   (%s)", "Agora: %s / %s   (%s)", "Sekarang: %s / %s   (%s)" },
		{ "Main",
		  "Ana Menü", "Principal", "Главное меню",
		  "الرئيسية", "主菜单", "メイン",
		  "Hauptmenü", "Menu Principal", "Menu Utama" },
		{ "Camera",
		  "Kamera", "Cámara", "Камера",
		  "الكاميرا", "摄像机", "カメラ",
		  "Kamera", "Câmera", "Kamera" },
		{ "Colours",
		  "Renkler", "Colores", "Цвета",
		  "الألوان", "颜色", "カラー",
		  "Farben", "Cores", "Warna" },
		{ "Key Bindings",
		  "Tuş Atamaları", "Teclas", "Клавиши",
		  "تظبيط الزراير", "按键设置", "キー設定",
		  "Tastenbelegung", "Atalhos de Teclas", "Pengaturan Tombol" },
		{ "Settings",
		  "Ayarlar", "Ajustes", "Настройки",
		  "الإعدادات", "设置", "設定",
		  "Einstellungen", "Configurações", "Pengaturan" },
		{ "About",
		  "Hakkında", "Acerca de", "О моде",
		  "عن المود", "关于", "このMODについて",
		  "Über", "Sobre", "Tentang" },
		{ "Open Key",
		  "Açma Tuşu", "Tecla de apertura", "Клавиша меню",
		  "زرار الفتح", "打开按键", "開くキー",
		  "Menütaste", "Tecla de Abrir", "Tombol Buka" },
		{ "VEHICLE",
		  "ARAÇ", "VEHÍCULO", "ТРАНСПОРТ",
		  "المركبة", "载具", "車両",
		  "FAHRZEUG", "VEÍCULO", "KENDARAAN" },
		{ "ON FOOT",
		  "YAYA", "A PIE", "ПЕШКОМ",
		  "على رجلك", "步行", "徒歩",
		  "ZU FUSS", "A PÉ", "JALAN KAKI" },
		{ "first person",
		  "birinci şahıs", "primera persona", "от первого лица",
		  "منظور أول", "第一人称", "一人称",
		  "Ego-Perspektive", "primeira pessoa", "orang pertama" },
		{ "third person",
		  "üçüncü şahıs", "tercera persona", "от третьего лица",
		  "منظور تالت", "第三人称", "三人称",
		  "Verfolgerperspektive", "terceira pessoa", "orang ketiga" },

		// ---- view / pov ------------------------------------------------
		{ "--- VIEW / POV ---",
		  "--- GÖRÜNÜM / BAKIŞ ---", "--- VISTA / PUNTO DE VISTA ---", "--- ВИД / ОБЗОР ---",
		  "--- المنظور / زاوية الرؤية ---", "--- 视角 / 视角模式 ---", "--- 視点 / POV ---",
		  "--- ANSICHT / POV ---", "--- VISÃO / POV ---", "--- TAMPILAN / POV ---" },
		{ "On-Foot View",
		  "Yaya Görünümü", "Vista a pie", "Вид пешком",
		  "منظور المشي", "步行视角", "徒歩の視点",
		  "Zu-Fuß-Ansicht", "Visão a Pé", "Kamera Jalan Kaki" },
		{ "Vehicle View",
		  "Araç Görünümü", "Vista en vehículo", "Вид в транспорте",
		  "منظور المركبة", "载具视角", "車両の視点",
		  "Fahrzeug-Ansicht", "Visão no Veículo", "Kamera Kendaraan" },
		{ "Reset View / POV",
		  "Görünümü Sıfırla", "Restablecer vista", "Сбросить вид",
		  "رجّع المنظور للأساسي", "重置视角", "視点をリセット",
		  "Ansicht / POV zurücksetzen", "Redefinir Visão / POV", "Reset Tampilan / POV" },
		{ "Don't touch",
		  "Dokunma", "No modificar", "Не менять",
		  "سيبه زي ما هو", "保持默认", "変更しない",
		  "Unverändert", "Não alterar", "Jangan ubah" },
		{ "Third Person",
		  "Üçüncü Şahıs", "Tercera persona", "Третье лицо",
		  "منظور تالت", "第三人称", "三人称",
		  "Dritte Person", "Terceira Pessoa", "Orang Ketiga" },
		{ "First Person",
		  "Birinci Şahıs", "Primera persona", "Первое лицо",
		  "منظور أول", "第一人称", "一人称",
		  "Erste Person", "Primeira Pessoa", "Orang Pertama" },

		// ---- on foot, third person -------------------------------------
		{ "--- ON FOOT - THIRD PERSON ---",
		  "--- YAYA - ÜÇÜNCÜ ŞAHIS ---", "--- A PIE - TERCERA PERSONA ---", "--- ПЕШКОМ - ТРЕТЬЕ ЛИЦО ---",
		  "--- على رجلك - منظور تالت ---", "--- 步行 - 第三人称 ---", "--- 徒歩 - 三人称 ---",
		  "--- ZU FUSS - DRITTE PERSON ---", "--- A PÉ - TERCEIRA PESSOA ---", "--- JALAN KAKI - ORANG KETIGA ---" },
		{ "Reset On-Foot Third Person",
		  "Yaya Üçüncü Şahsı Sıfırla", "Restablecer a pie (3.ª persona)", "Сбросить: пешком (3-е лицо)",
		  "رجّع المشي (منظور تالت) للأساسي", "重置步行第三人称", "徒歩三人称をリセット",
		  "Zu Fuß (3. Person) zurücksetzen", "Redefinir A Pé (3.ª Pessoa)", "Reset Jalan Kaki Orang Ketiga" },
		{ "Field Of View",
		  "Görüş Alanı (FOV)", "Campo de visión (FOV)", "Угол обзора (FOV)",
		  "مجال الرؤية (FOV)", "视野 (FOV)", "視野 (FOV)",
		  "Sichtfeld (FOV)", "Campo de Visão (FOV)", "Sudut Pandang (FOV)" },
		{ "Follow Distance",
		  "Takip Mesafesi", "Distancia de seguimiento", "Дистанция камеры",
		  "بعد الكاميرا", "跟随距离", "追従距離",
		  "Kameradistanz", "Distância da Câmera", "Jarak Kamera" },
		{ "Pivot Offset",
		  "Pivot Kayması", "Desplazamiento del pivote", "Смещение центра",
		  "تحريك المركز", "轴心偏移", "ピボット偏移",
		  "Drehpunkt-Versatz", "Deslocamento do Pivô", "Offset Pivot" },
		{ "Tilt Limit",
		  "Eğim Sınırı", "Límite de inclinación", "Предел наклона",
		  "أقصى ميل", "俯仰限制", "チルト上限",
		  "Neigungslimit", "Limite de Inclinação", "Batas Kemiringan" },
		{ "Keep FOV While Aiming",
		  "Nişanda Görüş Alanını Koru", "Mantener FOV al apuntar", "Сохранять FOV при прицеливании",
		  "ثبّت الـ FOV وقت التنشين", "瞄准时保持视野", "エイム中も視野を維持",
		  "Sichtfeld beim Zielen behalten", "Manter FOV ao Mirar", "Pertahankan FOV saat Membidik" },
		{ "On = same FOV down the sights. Off = aim stays vanilla.",
		  "Açık = nişanda aynı görüş alanı. Kapalı = nişan orijinal kalır.", "Activado: mismo FOV al apuntar. Desactivado: apuntado original.", "Вкл. — тот же FOV при прицеливании. Выкл. — стандартный.",
		  "شغال = نفس الـ FOV وقت التنشين. مقفول = التنشين يفضل عادي.", "开启 = 瞄准时保持相同视野。关闭 = 瞄准保持原版。", "オン = エイム中も同じ視野。オフ = 標準のまま。",
		  "Ein = gleiches Sichtfeld beim Zielen. Aus = Standard-Ziel-FOV.", "Ligado = mesmo FOV ao mirar. Desligado = mira padrão.", "Aktif = FOV sama saat membidik. Nonaktif = bidikan standar." },
		{ "Tilt Limit Up",
		  "Eğim Sınırı (Yukarı)", "Límite de inclinación (arriba)", "Предел наклона (вверх)",
		  "أقصى ميل لفوق", "仰角上限", "チルト上限（上）",
		  "Neigungslimit (oben)", "Limite de Inclinação (acima)", "Batas Kemiringan (Atas)" },
		{ "Tilt Limit Down",
		  "Eğim Sınırı (Aşağı)", "Límite de inclinación (abajo)", "Предел наклона (вниз)",
		  "أقصى ميل لتحت", "俯角下限", "チルト上限（下）",
		  "Neigungslimit (unten)", "Limite de Inclinação (abaixo)", "Batas Kemiringan (Bawah)" },
		{ "While Aiming",
		  "Nişan Alırken", "Al apuntar", "При прицеливании",
		  "وانت بتنشّن", "瞄准时", "エイム中",
		  "Beim Zielen", "Ao Mirar", "Saat Membidik" },
		{ "Aiming Field Of View",
		  "Nişan Görüş Alanı", "Campo de visión al apuntar", "Угол обзора при прицеливании",
		  "مجال الرؤية وقت التنشين", "瞄准视野", "エイム視野",
		  "Sichtfeld beim Zielen", "Campo de Visão ao Mirar", "FOV saat Membidik" },
		{ "Aiming Horizontal Origin",
		  "Nişan Yatay Merkezi", "Origen horizontal al apuntar", "Горизонтальное смещение при прицеливании",
		  "تظبيط المركز بالعرض وقت التنشين", "瞄准水平原点", "エイム水平原点",
		  "Horizontaler Ursprung beim Zielen", "Origem Horizontal ao Mirar", "Posisi Horizontal saat Membidik" },
		{ "Aiming Follow Distance",
		  "Nişan Takip Mesafesi", "Distancia al apuntar", "Дистанция при прицеливании",
		  "بعد الكاميرا وقت التنشين", "瞄准跟随距离", "エイム追従距離",
		  "Kameradistanz beim Zielen", "Distância ao Mirar", "Jarak Kamera saat Membidik" },
		{ "0 = the same as third person.",
		  "0 = üçüncü şahıs ile aynı.", "0 = igual que en tercera persona.", "0 = как в третьем лице.",
		  "0 = زي المنظور التالت.", "0 = 与第三人称相同。", "0 = 三人称と同じ。",
		  "0 = wie in der dritten Person.", "0 = igual à terceira pessoa.", "0 = sama dengan orang ketiga." },
		{ "Camera Shake",
		  "Kamera Sarsıntısı", "Vibración de cámara", "Тряска камеры",
		  "هزة الكاميرا", "镜头晃动", "カメラの揺れ",
		  "Kamerawackeln", "Tremor da Câmera", "Guncangan Kamera" },
		{ "Unarmed Reticle",
		  "Silahsız Nişangah", "Retícula sin arma", "Прицел без оружия",
		  "نقطة التنشين بدون سلاح", "空手准星", "素手時の照準",
		  "Fadenkreuz ohne Waffe", "Mira sem Arma", "Crosshair Tanpa Senjata" },
		{ "Keep First Person In Water",
		  "Suda Birinci Şahıs Kalsın", "Mantener 1.ª persona en el agua", "Не сбрасывать 1-е лицо в воде",
		  "خليك منظور أول في المية", "入水时保持第一人称", "水中でも一人称を維持",
		  "Erste Person im Wasser behalten", "Manter 1.ª Pessoa na Água", "Tetap Orang Pertama di Air" },
		{ "Keep First Person When Wrecked",
		  "Araç Hasarında Birinci Şahıs Kalsın", "Mantener 1.ª persona si se destruye", "Не сбрасывать 1-е лицо при поломке",
		  "خليك منظور أول لما المركبة تتدمر", "报废时保持第一人称", "大破時も一人称を維持",
		  "Erste Person bei Totalschaden behalten", "Manter 1.ª Pessoa ao Destruir", "Tetap Orang Pertama saat Hancur" },
		{ "The game normally forces third person for both.",
		  "Oyun normalde her ikisinde de üçüncü şahsa zorlar.",
		  "El juego normalmente fuerza la tercera persona en ambos casos.",
		  "Обычно игра принудительно включает третье лицо в обоих случаях.",
		  "اللعبة في العادي بترجعك منظور تالت في الحالتين دول.",
		  "游戏通常在这两种情况下强制切换为第三人称。",
		  "通常はどちらも三人称へ強制的に切り替わります。",
		  "Das Spiel erzwingt sonst in beiden Fällen die dritte Person.",
		  "O jogo normalmente força a terceira pessoa nos dois casos.",
		  "Game biasanya memaksa mode orang ketiga untuk keduanya." },
		{ "0 = leave the game's value alone.",
		  "0 = oyunun değerine dokunma.", "0 = mantener el valor del juego.", "0 = оставить значение игры.",
		  "0 = سيب قيمة اللعبة زي ما هي.", "0 = 不改动游戏原值。", "0 = ゲームの値はそのまま。",
		  "0 = Spielwert unverändert lassen.", "0 = manter o valor do jogo.", "0 = biarkan nilai bawaan game." },
		{ "0 = leave alone, 100 = the game's own amount.",
		  "0 = dokunma, 100 = oyunun kendi miktarı.", "0 = sin cambios, 100 = nivel del juego.",
		  "0 = не трогать, 100 = стандарт игры.",
		  "0 = متلمسوش، 100 = زي مستوى اللعبة.", "0 = 不改动，100 = 游戏原值。", "0 = そのまま、100 = ゲーム本来の量。",
		  "0 = unverändert, 100 = Spielwert.", "0 = não alterar, 100 = valor do jogo.", "0 = biarkan, 100 = nilai asli game." },
		{ "0 = off. 100 is a clearly visible shake.",
		  "0 = kapalı. 100 = belirgin sarsıntı.", "0 = desactivado. 100 = vibración bien visible.",
		  "0 = выкл. 100 = заметная тряска.",
		  "0 = مقفول. 100 = هزة واضحة جداً.", "0 = 关闭，100 = 明显可见的晃动。", "0 = オフ、100 = はっきり分かる揺れ。",
		  "0 = aus, 100 = deutlich sichtbares Wackeln.", "0 = desligado, 100 = tremor bem visível.",
		  "0 = nonaktif, 100 = guncangan terlihat jelas." },
		{ "Default",
		  "Varsayılan", "Predeterminado", "По умолчанию",
		  "الافتراضي", "默认", "既定",
		  "Standard", "Padrão", "Bawaan" },
		{ "Always",
		  "Her Zaman", "Siempre", "Всегда",
		  "دايمًا", "始终", "常に",
		  "Immer", "Sempre", "Selalu" },
		{ "Never",
		  "Asla", "Nunca", "Никогда",
		  "لأ خالص", "从不", "なし",
		  "Nie", "Nunca", "Tidak Pernah" },
		// ---- on foot, first person -------------------------------------
		{ "--- ON FOOT - FIRST PERSON ---",
		  "--- YAYA - BİRİNCİ ŞAHIS ---", "--- A PIE - PRIMERA PERSONA ---", "--- ПЕШКОМ - ПЕРВОЕ ЛИЦО ---",
		  "--- على رجلك - منظور أول ---", "--- 步行 - 第一人称 ---", "--- 徒歩 - 一人称 ---",
		  "--- ZU FUSS - ERSTE PERSON ---", "--- A PÉ - PRIMEIRA PESSOA ---", "--- JALAN KAKI - ORANG PERTAMA ---" },
		{ "Reset On-Foot First Person",
		  "Yaya Birinci Şahsı Sıfırla", "Restablecer a pie (1.ª persona)", "Сбросить: пешком (1-е лицо)",
		  "رجّع المشي (منظور أول) للأساسي", "重置步行第一人称", "徒歩一人称をリセット",
		  "Zu Fuß (1. Person) zurücksetzen", "Redefinir A Pé (1.ª Pessoa)", "Reset Jalan Kaki Orang Pertama" },

		// ---- vehicle, third person -------------------------------------
		{ "--- VEHICLE - THIRD PERSON ---",
		  "--- ARAÇ - ÜÇÜNCÜ ŞAHIS ---", "--- VEHÍCULO - TERCERA PERSONA ---", "--- ТРАНСПОРТ - ТРЕТЬЕ ЛИЦО ---",
		  "--- المركبة - منظور تالت ---", "--- 载具 - 第三人称 ---", "--- 車両 - 三人称 ---",
		  "--- FAHRZEUG - DRITTE PERSON ---", "--- VEÍCULO - TERCEIRA PESSOA ---", "--- KENDARAAN - ORANG KETIGA ---" },
		{ "Reset Vehicle Third Person",
		  "Araç Üçüncü Şahsı Sıfırla", "Restablecer vehículo (3.ª persona)", "Сбросить: транспорт (3-е лицо)",
		  "رجّع المركبة (منظور تالت) للأساسي", "重置载具第三人称", "車両三人称をリセット",
		  "Fahrzeug (3. Person) zurücksetzen", "Redefinir Veículo (3.ª Pessoa)", "Reset Kendaraan Orang Ketiga" },
		{ "Pivot Scale",
		  "Pivot Ölçeği", "Escala del pivote", "Масштаб центра",
		  "حجم المحور", "轴心比例", "ピボット倍率",
		  "Drehpunkt-Skala", "Escala do Pivô", "Skala Pivot" },
		{ "Follow Zoom",
		  "Takip Yakınlaştırması", "Zoom de seguimiento", "Приближение камеры",
		  "زوم الكاميرا", "跟随缩放", "追従ズーム",
		  "Kamera-Zoom", "Zoom da Câmera", "Zoom Kamera" },

		// ---- vehicle, first person -------------------------------------
		{ "--- VEHICLE - FIRST PERSON ---",
		  "--- ARAÇ - BİRİNCİ ŞAHIS ---", "--- VEHÍCULO - PRIMERA PERSONA ---", "--- ТРАНСПОРТ - ПЕРВОЕ ЛИЦО ---",
		  "--- المركبة - منظور أول ---", "--- 载具 - 第一人称 ---", "--- 車両 - 一人称 ---",
		  "--- FAHRZEUG - ERSTE PERSON ---", "--- VEÍCULO - PRIMEIRA PESSOA ---", "--- KENDARAAN - ORANG PERTAMA ---" },
		{ "Reset Vehicle First Person",
		  "Araç Birinci Şahsı Sıfırla", "Restablecer vehículo (1.ª persona)", "Сбросить: транспорт (1-е лицо)",
		  "رجّع المركبة (منظور أول) للأساسي", "重置载具第一人称", "車両一人称をリセット",
		  "Fahrzeug (1. Person) zurücksetzen", "Redefinir Veículo (1.ª Pessoa)", "Reset Kendaraan Orang Pertama" },
		{ "View Offset Z",
		  "Görüş Kayması Z", "Desplazamiento vertical Z", "Смещение обзора по Z",
		  "رفع/نزول الكاميرا Z", "视角高度偏移 Z", "視点オフセットZ",
		  "Ansicht-Versatz Z", "Deslocamento da Visão Z", "Offset Tampilan Z" },

		// ---- weapons ----------------------------------------------------
		{ "Weapons",
		  "Silahlar", "Armas", "Оружие",
		  "الأسلحة", "武器", "武器",
		  "Waffen", "Armas", "Senjata" },
		{ "Reset Weapons",
		  "Silahları Sıfırla", "Restablecer armas", "Сбросить оружие",
		  "رجّع الأسلحة للأساسي", "重置武器设置", "武器設定をリセット",
		  "Waffen zurücksetzen", "Redefinir Armas", "Reset Senjata" },
		{ "POV of the camera the game aims each weapon through.",
		  "Her silahla nişan alırken oyunun kullandığı kamera açısı.", "Campo de visión de la cámara con la que apunta cada arma.", "Угол обзора камеры при прицеливании из каждого вида оружия.",
		  "مجال رؤية الكاميرا اللي اللعبة بتنشّن بيها بكل سلاح.", "游戏为每种武器瞄准时使用的摄像机视野。", "各武器のエイム時にゲームが使うカメラの視野。",
		  "Sichtfeld der Kamera, mit der das Spiel jede Waffe zielt.", "FOV da câmera usada ao mirar com cada arma.", "FOV kamera yang dipakai game saat membidik tiap senjata." },
		{ "0 = leave the game's value alone. Only used while aiming.",
		  "0 = oyunun değerine dokunma. Yalnızca nişan alırken geçerlidir.", "0 = mantener el valor del juego. Solo se aplica al apuntar.", "0 = не менять значение игры. Действует только при прицеливании.",
		  "0 = سيب قيمة اللعبة زي ما هي. شغال وقت التنشين بس.", "0 = 不改动游戏原值。仅在瞄准时生效。", "0 = ゲームの値はそのまま。エイム時のみ適用されます。",
		  "0 = Spielwert unverändert lassen. Gilt nur beim Zielen.", "0 = manter o valor do jogo. Aplicado apenas ao mirar.", "0 = biarkan nilai game. Hanya berlaku saat membidik." },
		{ "Default (all other weapons)",
		  "Varsayılan (diğer tüm silahlar)", "Predeterminada (el resto de armas)", "По умолчанию (остальное оружие)",
		  "الأساسي (باقي الأسلحة كلها)", "默认（其他所有武器）", "既定（その他の武器すべて）",
		  "Standard (alle anderen Waffen)", "Padrão (todas as outras armas)", "Bawaan (semua senjata lain)" },
		{ "Hip Fire",
		  "Kalçadan Ateş", "Disparo desde la cadera", "Стрельба от бедра",
		  "ضرب من غير تنشين", "腰射", "腰だめ撃ち",
		  "Hüftfeuer", "Tiro sem Mira", "Tembak dari Pinggang" },
		{ "RPG / Launchers",
		  "RPG / Roketatarlar", "RPG / Lanzacohetes", "РПГ / Гранатомёты",
		  "آر بي جي / قاذفات", "RPG / 发射器", "RPG / ランチャー",
		  "RPG / Werfer", "RPG / Lançadores", "RPG / Peluncur" },
		{ "Shotgun",
		  "Pompalı Tüfek", "Escopeta", "Дробовик",
		  "شوتجن", "霰弹枪", "ショットガン",
		  "Schrotflinte", "Espingarda", "Shotgun" },
		{ "Thrown",
		  "Fırlatılanlar", "Arrojadizas", "Метательное",
		  "قنابل ومقذوفات", "投掷物", "投擲武器",
		  "Wurfwaffen", "Arremessáveis", "Senjata Lempar" },
		{ "Melee / Animals",
		  "Yakın Dövüş / Hayvanlar", "Cuerpo a cuerpo / Animales", "Ближний бой / Животные",
		  "ضرب بالايد / الحيوانات", "近战 / 动物", "近接 / 動物",
		  "Nahkampf / Tiere", "Corpo a Corpo / Animais", "Jarak Dekat / Hewan" },
		{ "Digiscanner",
		  "Digiscanner", "Escáner digital", "Дигисканер",
		  "جهاز المسح", "数字扫描仪", "デジスキャナー",
		  "Digiscanner", "Scanner Digital", "Digiscanner" },
		{ "Jerry Can",
		  "Benzin Bidonu", "Bidón de gasolina", "Канистра",
		  "جركن بنزين", "汽油桶", "ガソリン缶",
		  "Benzinkanister", "Galão de Gasolina", "Jeriken Bensin" },
		{ "Sniper",
		  "Keskin Nişancı", "Francotirador", "Снайперская винтовка",
		  "سنايبر", "狙击枪", "スナイパー",
		  "Scharfschützengewehr", "Rifle de Precisão", "Sniper" },
		{ "Sniper (Low Zoom)",
		  "Keskin Nişancı (Düşük Yakınlaştırma)", "Francotirador (zoom bajo)", "Снайперская (малый зум)",
		  "سنايبر (زوم قليل)", "狙击枪（低倍镜）", "スナイパー（低倍率）",
		  "Scharfschütze (wenig Zoom)", "Rifle de Precisão (Zoom Baixo)", "Sniper (Zoom Rendah)" },

		// ---- metadata layer --------------------------------------------
		{ "FOV / distance / pivot use the metadata layer.",
		  "Görüş alanı / mesafe / pivot metadata katmanını kullanır.", "FOV / distancia / pivote usan la capa de metadatos.", "Угол обзора / дистанция / центр работают через слой метаданных.",
		  "الـ FOV / البعد / المحور بيستخدموا طبقة الـ Metadata.", "视野 / 距离 / 轴心使用元数据层。", "視野 / 距離 / ピボットはメタデータ層を使用します。",
		  "Sichtfeld / Distanz / Drehpunkt nutzen die Metadaten-Ebene.", "FOV / distância / pivô usam a camada de metadados.", "FOV / jarak / pivot memakai lapisan metadata." },
		{ "Tilt Limit and Follow Zoom are plain natives.",
		  "Eğim Sınırı ve Takip Yakınlaştırması standart native işlevlerdir.", "El límite de inclinación y el zoom son funciones nativas.", "Предел наклона и приближение — обычные native-функции.",
		  "أقصى ميل وزوم الكاميرا شغالين بالـ Natives العادية.", "俯仰限制和跟随缩放是普通原生函数。", "チルト上限と追従ズームは通常のネイティブです。",
		  "Neigungslimit und Kamera-Zoom sind Standard-Natives.", "Limite de Inclinação e Zoom usam funções nativas simples.", "Batas Kemiringan dan Zoom Kamera memakai native biasa." },
		{ "Metadata Layer (FOV etc.)",
		  "Metadata Katmanı (FOV vb.)", "Capa de metadatos (FOV, etc.)", "Слой метаданных (FOV и т. д.)",
		  "طبقة الـ Metadata (الـ FOV وغيره)", "元数据层（视野等）", "メタデータ層（視野など）",
		  "Metadaten-Ebene (Sichtfeld usw.)", "Camada de Metadados (FOV etc.)", "Lapisan Metadata (FOV dll.)" },
		{ "metadata: active",
		  "metadata: etkin", "metadatos: activos", "метаданные: активны",
		  "metadata: شغالة", "元数据: 已激活", "メタデータ: 有効",
		  "Metadaten: aktiv", "metadados: ativos", "metadata: aktif" },
		{ "metadata: unavailable",
		  "metadata: kullanılamıyor", "metadatos: no disponibles", "метаданные: недоступны",
		  "metadata: مش متاحة", "元数据: 不可用", "メタデータ: 利用不可",
		  "Metadaten: nicht verfügbar", "metadados: indisponíveis", "metadata: tidak tersedia" },
		// Debug helper: writes the camera's metadata object to CamControl.log so a
		// new option's field offset can be found by diffing two dumps.
		{ "Dump Camera Data To Log",
		  "Kamera Verisini Günlüğe Yaz", "Volcar datos de cámara al registro", "Выгрузить данные камеры в лог",
		  "اكتب بيانات الكاميرا في ملف اللوج", "导出摄像机数据到日志", "カメラデータをログに出力",
		  "Kameradaten ins Log schreiben", "Gravar dados da câmera no log", "Simpan Data Kamera ke Log" },
		{ "Dev Options",
		  "Geliştirici Seçenekleri", "Opciones de desarrollador", "Параметры разработчика",
		  "إعدادات المطور", "开发者选项", "開発者オプション",
		  "Entwickleroptionen", "Opções de Desenvolvedor", "Opsi Pengembang" },
		// Live metadata probe - see README, "DEVELOPER OPTIONS".
		{ "Probe Camera Field",
		  "Kamera Alanını Test Et", "Sondear campo de cámara", "Проверить поле камеры",
		  "فحص خانة في الكاميرا", "探测摄像机字段", "カメラフィールドを調べる",
		  "Kamerafeld testen", "Sondar Campo da Câmera", "Uji Field Kamera" },
		{ "Probe Offset",
		  "Test Ofseti", "Desplazamiento de sondeo", "Смещение проверки",
		  "عنوان الخانة (Offset)", "探测偏移", "調査オフセット",
		  "Test-Offset", "Deslocamento da Sondagem", "Offset Pengujian" },
		{ "Probe Value",
		  "Test Değeri", "Valor de sondeo", "Значение проверки",
		  "قيمة الاختبار", "探测数值", "調査値",
		  "Test-Wert", "Valor da Sondagem", "Nilai Pengujian" },
		{ "Probe writes while the menu is open only.",
		  "Test yalnızca menü açıkken yazar.", "El sondeo solo escribe con el menú abierto.", "Проверка записывает только при открытом меню.",
		  "الفحص بيكتب والقائمة مفتوحة بس.", "仅在菜单打开时写入。", "メニューを開いている間だけ書き込みます。",
		  "Test schreibt nur bei offenem Menü.", "A sondagem grava apenas com o menu aberto.", "Pengujian hanya menulis saat menu terbuka." },
		{ "List Camera Hashes",
		  "Kamera Hash'lerini Listele", "Listar hashes de cámara", "Список хэшей камер",
		  "اعرض أكواد الكاميرات (Hashes)", "列出摄像机哈希", "カメラハッシュを一覧表示",
		  "Kamera-Hashes auflisten", "Listar Hashes da Câmera", "Daftar Hash Kamera" },
		// Automatic field sweep - see README, "DEVELOPER OPTIONS".
		{ "Sweep Camera Fields",
		  "Kamera Alanlarını Tara", "Explorar campos de cámara", "Сканировать поля камеры",
		  "فحص شامل لخانات الكاميرا", "扫描摄像机字段", "カメラフィールドを走査",
		  "Kamerafelder scannen", "Varrer Campos da Câmera", "Pindai Field Kamera" },
		{ "Sweep tests every offset while the menu is open.",
		  "Tarama, menü açıkken her konumu dener.", "La exploración prueba cada desplazamiento con el menú abierto.", "Сканирование проверяет каждое смещение при открытом меню.",
		  "الفحص بيجرّب كل خانة والقائمة مفتوحة.", "菜单打开时扫描会测试每一个偏移。", "メニューを開いている間、すべてのオフセットを試します。",
		  "Der Scan testet jeden Offset bei offenem Menü.", "A varredura testa cada deslocamento com o menu aberto.", "Pemindaian menguji tiap offset saat menu terbuka." },
		{ "Sweep runs until switched off.",
		  "Tarama kapatılana kadar çalışır.", "La exploración sigue hasta desactivarla.", "Сканирование идёт до отключения.",
		  "الفحص هيفضل شغال لحد ما تقفله.", "扫描会一直运行直到关闭。", "走査はオフにするまで続きます。",
		  "Scan läuft bis zum Ausschalten.", "A varredura continua até ser desligada.", "Pemindaian berjalan sampai dimatikan." },
		{ "Sweep Start Offset",
		  "Tarama Başlangıç Ofseti", "Inicio de exploración", "Начальное смещение сканирования",
		  "بداية الفحص (Offset)", "扫描起始偏移", "走査の開始位置",
		  "Scan-Start-Offset", "Deslocamento Inicial da Varredura", "Offset Awal Pemindaian" },
		{ "Disable Idle Animation",
		  "Boşta Animasyonu Kapat", "Desactivar animación en reposo", "Отключить анимацию ожидания",
		  "وقف حركات الوقوف (Idle)", "禁用待机动画", "待機アニメを無効化",
		  "Leerlauf-Animation deaktivieren", "Desativar Animação Ociosa", "Nonaktifkan Animasi Diam" },
		{ "metadata: active on this build.",
		  "metadata: bu sürümde etkin.", "metadatos: activos en esta versión.", "метаданные: доступны в этой сборке.",
		  "الـ Metadata: شغالة على النسخة دي.", "元数据: 在此版本可用。", "メタデータ: このビルドで有効。",
		  "Metadaten: in dieser Version aktiv.", "metadados: ativos nesta versão.", "metadata: aktif di build ini." },
		{ "metadata: NOT found - FOV sliders will do nothing.",
		  "metadata: BULUNAMADI - FOV kaydırıcıları çalışmaz.", "metadatos: NO encontrados; los controles de FOV no harán nada.", "метаданные: НЕ найдены — ползунки FOV работать не будут.",
		  "الـ Metadata: مش موجودة - شرايط الـ FOV مش هتشتغل.", "元数据: 未找到 - 视野滑块将无效。", "メタデータ: 見つかりません - 視野スライダーは無効です。",
		  "Metadaten: NICHT gefunden - Sichtfeld-Regler wirkungslos.", "metadados: NÃO encontrados - controles de FOV não farão nada.", "metadata: TIDAK ditemukan - slider FOV tidak akan berfungsi." },

		// ---- colours ----------------------------------------------------
		{ "RESET THIS PAGE",
		  "BU SAYFAYI SIFIRLA", "RESTABLECER ESTA PÁGINA", "СБРОСИТЬ ЭТУ СТРАНИЦУ",
		  "رجّع الصفحة دي للأساسي", "重置本页", "このページをリセット",
		  "DIESE SEITE ZURÜCKSETZEN", "REDEFINIR ESTA PÁGINA", "RESET HALAMAN INI" },
		{ "Pick a preset, or use Custom and set R/G/B.",
		  "Bir ön ayar seçin ya da Özel'i seçip R/G/B ayarlayın.", "Elige un preajuste o usa Personalizado y ajusta R/G/B.", "Выберите пресет или «Свой» и задайте R/G/B.",
		  "اختار لون جاهز أو اختار «مخصص» وظبط R/G/B.", "选择预设，或选择自定义并设置 R/G/B。", "プリセットを選ぶか、カスタムで R/G/B を設定します。",
		  "Voreinstellung wählen oder Benutzerdefiniert mit R/G/B nutzen.", "Escolha uma predefinição ou use Personalizado e ajuste R/G/B.", "Pilih preset, atau pilih Kustom lalu atur R/G/B." },
		{ "Preset",
		  "Ön Ayar", "Preajuste", "Пресет",
		  "لون جاهز", "预设", "プリセット",
		  "Voreinstellung", "Predefinição", "Preset" },
		{ "Red",
		  "Kırmızı", "Rojo", "Красный",
		  "أحمر", "红", "赤",
		  "Rot", "Vermelho", "Merah" },
		{ "Green",
		  "Yeşil", "Verde", "Зелёный",
		  "أخضر", "绿", "緑",
		  "Grün", "Verde", "Hijau" },
		{ "Blue",
		  "Mavi", "Azul", "Синий",
		  "أزرق", "蓝", "青",
		  "Blau", "Azul", "Biru" },
		{ "Text switches black/white on its own to stay readable.",
		  "Yazı okunabilirlik için siyah/beyaza otomatik geçer.", "El texto cambia entre negro y blanco para seguir siendo legible.", "Текст автоматически меняется на чёрный/белый для читаемости.",
		  "لون الكلام بيتغير لوحده أبيض أو أسود عشان يفضل مقري.", "文字会自动切换黑白以保持清晰。", "文字は読みやすさのため白黒を自動で切り替えます。",
		  "Text wechselt automatisch zwischen Schwarz und Weiß für gute Lesbarkeit.", "O texto alterna entre preto e branco sozinho para ficar legível.", "Teks otomatis berganti hitam/putih agar tetap mudah dibaca." },
		{ "Re-apply Selected Preset",
		  "Seçili Ön Ayarı Yeniden Uygula", "Reaplicar preajuste", "Применить пресет заново",
		  "طبّق اللون المختار تاني", "重新应用所选预设", "選択中のプリセットを再適用",
		  "Gewählte Voreinstellung erneut anwenden", "Reaplicar Predefinição Selecionada", "Terapkan Ulang Preset Terpilih" },
		{ "Moving an RGB slider switches the preset to Custom.",
		  "Bir RGB kaydırıcısını oynatmak ön ayarı Özel yapar.", "Mover un control R/G/B cambia el preajuste a Personalizado.", "Изменение ползунка R/G/B переключит пресет на «Свой».",
		  "لو حركت أي شريط RGB هيتحول لـ «مخصص».", "移动 RGB 滑块会把预设切换为自定义。", "RGBスライダーを動かすとプリセットがカスタムになります。",
		  "Ein RGB-Regler schaltet auf Benutzerdefiniert um.", "Mover um controle RGB muda a predefinição para Personalizado.", "Menggeser slider RGB akan mengubah preset ke Kustom." },
		{ "Custom (RGB below)",
		  "Özel (aşağıda R/G/B)", "Personalizado (R/G/B abajo)", "Свой (R/G/B ниже)",
		  "مخصص (R/G/B تحت)", "自定义（下方 RGB）", "カスタム（下のRGB）",
		  "Benutzerdefiniert (RGB unten)", "Personalizado (RGB abaixo)", "Kustom (RGB di bawah)" },
		{ "Monochrome",
		  "Tek Renk", "Monocromo", "Монохром",
		  "أبيض وأسود", "单色", "モノクロ",
		  "Monochrom", "Monocromático", "Monokrom" },
		{ "Amber",
		  "Kehribar", "Ámbar", "Янтарный",
		  "كهرماني", "琥珀", "アンバー",
		  "Bernstein", "Âmbar", "Amber" },
		{ "Emerald",
		  "Zümrüt", "Esmeralda", "Изумрудный",
		  "زمردي", "翠绿", "エメラルド",
		  "Smaragd", "Esmeralda", "Zamrud" },
		{ "Cyan",
		  "Camgöbeği", "Cian", "Голубой",
		  "سماوي", "青色", "シアン",
		  "Cyan", "Ciano", "Sian" },
		{ "Crimson",
		  "Kızıl", "Carmesí", "Багровый",
		  "أحمر غامق", "绯红", "クリムゾン",
		  "Karmesin", "Carmesim", "Merah Tua" },
		{ "Violet",
		  "Mor", "Violeta", "Фиолетовый",
		  "بنفسجي", "紫罗兰", "バイオレット",
		  "Violett", "Violeta", "Ungu" },

		// ---- keys -------------------------------------------------------
		{ "Select a row, then press a key. ESC cancels.",
		  "Bir satır seçin, sonra bir tuşa basın. ESC iptal eder.", "Selecciona una fila y pulsa una tecla. ESC cancela.", "Выберите строку и нажмите клавишу. ESC — отмена.",
		  "اختار سطر وبعدين دوس أي زرار. ESC بيلغي.", "选择一行，然后按下按键。ESC 取消。", "行を選んでキーを押します。ESCで取消。",
		  "Zeile wählen, dann Taste drücken. ESC bricht ab.", "Selecione uma linha e pressione uma tecla. ESC cancela.", "Pilih baris, lalu tekan tombol. ESC membatalkan." },
		{ "Left/Right repeat while held - just hold them.",
		  "Sol/Sağ basılı tutunca tekrarlar - basılı tutun.", "Izquierda/Derecha se repiten al mantenerlos pulsados.", "Влево/Вправо повторяются при удержании.",
		  "يمين وشمال بيتكرروا لو فضلت دايس عليهم.", "按住左/右会连续调整。", "左/右は押しっぱなしで連続変化します。",
		  "Links/Rechts wiederholen beim Gedrückthalten.", "Esquerda/Direita repetem ao segurar - basta manter pressionado.", "Kiri/Kanan berulang saat ditahan - cukup tahan tombolnya." },
		{ "Open Menu (primary)",
		  "Menüyü Aç (ana)", "Abrir menú (principal)", "Открыть меню (основная)",
		  "فتح القائمة (أساسي)", "打开菜单（主）", "メニューを開く（メイン）",
		  "Menü öffnen (primär)", "Abrir Menu (principal)", "Buka Menu (utama)" },
		{ "Open Menu (extra 1)",
		  "Menüyü Aç (ek 1)", "Abrir menú (extra 1)", "Открыть меню (доп. 1)",
		  "فتح القائمة (إضافي 1)", "打开菜单（备用 1）", "メニューを開く（予備1）",
		  "Menü öffnen (extra 1)", "Abrir Menu (extra 1)", "Buka Menu (ekstra 1)" },
		{ "Open Menu (extra 2)",
		  "Menüyü Aç (ek 2)", "Abrir menú (extra 2)", "Открыть меню (доп. 2)",
		  "فتح القائمة (إضافي 2)", "打开菜单（备用 2）", "メニューを開く（予備2）",
		  "Menü öffnen (extra 2)", "Abrir Menu (extra 2)", "Buka Menu (ekstra 2)" },
		{ "Navigate Up",
		  "Yukarı Git", "Subir", "Вверх",
		  "طلع لفوق", "上移", "上へ",
		  "Nach oben", "Mover para Cima", "Navigasi Atas" },
		{ "Navigate Down",
		  "Aşağı Git", "Bajar", "Вниз",
		  "انزل لتحت", "下移", "下へ",
		  "Nach unten", "Mover para Baixo", "Navigasi Bawah" },
		{ "Decrease / Left",
		  "Azalt / Sol", "Reducir / Izquierda", "Уменьшить / Влево",
		  "قلّل / شمال", "减少 / 左", "減らす / 左",
		  "Verringern / Links", "Diminuir / Esquerda", "Kurangi / Kiri" },
		{ "Increase / Right",
		  "Artır / Sağ", "Aumentar / Derecha", "Увеличить / Вправо",
		  "زوّد / يمين", "增加 / 右", "増やす / 右",
		  "Erhöhen / Rechts", "Aumentar / Direita", "Tambah / Kanan" },
		{ "Select",
		  "Seç", "Seleccionar", "Выбрать",
		  "اختيار", "选择", "決定",
		  "Auswählen", "Selecionar", "Pilih" },
		{ "Back",
		  "Geri", "Atrás", "Назад",
		  "رجوع", "返回", "戻る",
		  "Zurück", "Voltar", "Kembali" },
		{ "Arrows + Enter + Backspace always work as a fallback.",
		  "Oklar + Enter + Backspace her zaman yedek olarak çalışır.", "Las flechas + Intro + Retroceso siempre funcionan como respaldo.", "Стрелки + Enter + Backspace всегда работают как запасной вариант.",
		  "الأسهم + Enter + Backspace شغالين دايمًا كاحتياطي.", "方向键 + Enter + Backspace 始终可用作备用。", "矢印 + Enter + Backspace は常に予備として機能します。",
		  "Pfeile + Enter + Backspace funktionieren immer als Fallback.", "Setas + Enter + Backspace sempre funcionam como alternativa.", "Panah + Enter + Backspace selalu berfungsi sebagai cadangan." },
		{ "Restore Default Keys",
		  "Varsayılan Tuşlara Dön", "Restaurar teclas", "Сбросить клавиши",
		  "رجّع الزراير للأساسي", "恢复默认按键", "既定のキーに戻す",
		  "Standardtasten wiederherstellen", "Restaurar Teclas Padrão", "Pulihkan Tombol Bawaan" },

		// ---- settings ---------------------------------------------------
		{ "Auto Save Config",
		  "Otomatik Kaydet", "Guardado automático", "Автосохранение",
		  "حفظ تلقائي", "自动保存配置", "自動保存",
		  "Automatisch speichern", "Salvar Automaticamente", "Simpan Otomatis" },
		{ "Debug Overlay",
		  "Hata Ayıklama Paneli", "Panel de depuración", "Отладочная панель",
		  "شاشة المعلومات", "调试面板", "デバッグ表示",
		  "Debug-Overlay", "Painel de Depuração", "Overlay Debug" },
		{ "Save Config Now",
		  "Şimdi Kaydet", "Guardar ahora", "Сохранить сейчас",
		  "احفظ الإعدادات دلوقتي", "立即保存配置", "今すぐ保存",
		  "Konfiguration jetzt speichern", "Salvar Configuração Agora", "Simpan Konfigurasi Sekarang" },
		{ "Reload Config From Disk",
		  "Diskten Yeniden Yükle", "Recargar desde disco", "Загрузить с диска",
		  "حمّل الإعدادات من الملف", "从磁盘重新加载", "ディスクから再読込",
		  "Konfiguration neu laden", "Recarregar do Disco", "Muat Ulang dari Disk" },
		{ "Reset All To Defaults",
		  "Hepsini Varsayılana Döndür", "Restablecer todo", "Сбросить всё",
		  "رجّع كل حاجة للأساسي", "全部恢复默认", "すべて既定に戻す",
		  "Alles zurücksetzen", "Redefinir Tudo para o Padrão", "Reset Semua ke Bawaan" },
		{ "Files sit next to CamControl.asi:",
		  "Dosyalar CamControl.asi yanındadır:", "Los archivos están junto a CamControl.asi:", "Файлы лежат рядом с CamControl.asi:",
		  "الملفات موجودة جنب CamControl.asi:", "文件位于 CamControl.asi 旁边：", "ファイルは CamControl.asi の隣にあります:",
		  "Dateien liegen neben CamControl.asi:", "Arquivos ficam ao lado de CamControl.asi:", "File berada di sebelah CamControl.asi:" },
		{ "CamControl.ini   (config)",
		  "CamControl.ini   (ayarlar)", "CamControl.ini   (configuración)", "CamControl.ini   (настройки)",
		  "CamControl.ini   (الإعدادات)", "CamControl.ini   (配置)", "CamControl.ini   (設定)",
		  "CamControl.ini   (Konfiguration)", "CamControl.ini   (config)", "CamControl.ini   (konfigurasi)" },
		{ "CamControl.log   (log)",
		  "CamControl.log   (günlük)", "CamControl.log   (registro)", "CamControl.log   (журнал)",
		  "CamControl.log   (السجل)", "CamControl.log   (日志)", "CamControl.log   (ログ)",
		  "CamControl.log   (Log)", "CamControl.log   (log)", "CamControl.log   (log)" },
		{ "Language",
		  "Dil", "Idioma", "Язык",
		  "اللغة", "语言", "言語",
		  "Sprache", "Idioma", "Bahasa" },
		{ "Applies to the menu and the on-screen text.",
		  "Menüye ve ekrandaki yazılara uygulanır.", "Se aplica al menú y al texto en pantalla.", "Применяется к меню и тексту на экране.",
		  "بيتطبق على القائمة والكلام اللي على الشاشة.", "应用于菜单和屏幕文字。", "メニューと画面上の文字に適用されます。",
		  "Gilt für Menü und Bildschirmtext.", "Aplica-se ao menu e ao texto na tela.", "Berlaku untuk menu dan teks di layar." },
		{ "Needs a restart to take full effect.",
		  "Tam etki için yeniden başlatma gerekir.", "Requiere reiniciar para aplicarse por completo.", "Для полного применения нужен перезапуск.",
		  "لازم ترستر اللعبة عشان يشتغل بالكامل.", "需要重启才能完全生效。", "完全に反映するには再起動が必要です。",
		  "Erfordert einen Neustart für volle Wirkung.", "Precisa reiniciar para efeito completo.", "Perlu restart agar berlaku penuh." },

		// ---- language names (shown in the Language row) ------------------
		{ "English",
		  "İngilizce", "Inglés", "Английский",
		  "إنجليزي", "英语", "英語",
		  "Englisch", "Inglês", "Inggris" },
		{ "Turkish",
		  "Türkçe", "Turco", "Турецкий",
		  "تركي", "土耳其语", "トルコ語",
		  "Türkisch", "Turco", "Turki" },
		{ "Spanish",
		  "İspanyolca", "Español", "Испанский",
		  "إسباني", "西班牙语", "スペイン語",
		  "Spanisch", "Espanhol", "Spanyol" },
		{ "Russian",
		  "Rusça", "Ruso", "Русский",
		  "روسي", "俄语", "ロシア語",
		  "Russisch", "Russo", "Rusia" },
		{ "Egyptian Arabic",
		  "Mısır Arapçası", "Árabe egipcio", "Египетский арабский",
		  "عربي مصري", "埃及阿拉伯语", "エジプト・アラビア語",
		  "Ägyptisches Arabisch", "Árabe Egípcio", "Arab Mesir" },
		{ "Chinese",
		  "Çince", "Chino", "Китайский",
		  "صيني", "中文", "中国語",
		  "Chinesisch", "Chinês", "Tionghoa" },
		{ "Japanese",
		  "Japonca", "Japonés", "Японский",
		  "ياباني", "日语", "日本語",
		  "Japanisch", "Japonês", "Jepang" },
		{ "German",
		  "Almanca", "Alemán", "Немецкий",
		  "ألماني", "德语", "ドイツ語",
		  "Deutsch", "Alemão", "Jerman" },
		{ "Portuguese",
		  "Portekizce", "Portugués", "Португальский",
		  "برتغالي", "葡萄牙语", "ポルトガル語",
		  "Portugiesisch", "Português", "Portugis" },
		{ "Indonesian",
		  "Endonezce", "Indonesio", "Индонезийский",
		  "إندونيسي", "印度尼西亚语", "インドネシア語",
		  "Indonesisch", "Indonésio", "Indonesia" },

		// ---- about ------------------------------------------------------
		{ "made by sensin",
		  "yapımcı: sensin", "hecho por sensin", "сделал sensin",
		  "من صنع sensin", "作者: sensin", "制作: sensin",
		  "erstellt von sensin", "feito por sensin", "dibuat oleh sensin" },
		{ "Version",
		  "Sürüm", "Versión", "Версия",
		  "الإصدار", "版本", "バージョン",
		  "Version", "Versão", "Versi" },
		{ "Camera control for GTA V, single player.",
		  "GTA V tek oyunculu için kamera kontrolü.", "Control de cámara para GTA V (un jugador).", "Управление камерой для GTA V (одиночная игра).",
		  "تحكم في كاميرا GTA V، أوفلاين.", "GTA V 单人模式的摄像机控制。", "GTA V シングルプレイ用カメラ操作。",
		  "Kamerasteuerung für GTA V (Einzelspieler).", "Controle de câmera para GTA V (um jogador).", "Kontrol kamera untuk GTA V (single player)." },
		{ "Built with the ScriptHookV SDK.",
		  "ScriptHookV SDK ile geliştirildi.", "Desarrollado con el SDK de ScriptHookV.", "Собрано на ScriptHookV SDK.",
		  "مبني باستخدام ScriptHookV SDK.", "使用 ScriptHookV SDK 构建。", "ScriptHookV SDK を使用。",
		  "Erstellt mit dem ScriptHookV SDK.", "Feito com o ScriptHookV SDK.", "Dibuat dengan ScriptHookV SDK." },
		{ "Drives the game's own camera, so mouse look,",
		  "Oyunun kendi kamerasını kullanır; fare bakışı,", "Usa la cámara del juego, así el ratón,", "Использует камеру игры, поэтому обзор мышью,",
		  "بيستخدم كاميرا اللعبة نفسها، فحركة الماوس،", "驱动游戏自带摄像机，因此鼠标视角、", "ゲーム本来のカメラを使うため、マウス視点、",
		  "Steuert die Spielkamera, damit Mausblick,", "Controla a câmera do jogo, então o mouse,", "Menggerakkan kamera asli game, jadi mouse look," },
		{ "walking, aiming and driving stay normal.",
		  "yürüme, nişan ve sürüş normal kalır.", "caminar, apuntar y conducir siguen normales.", "ходьба, прицеливание и вождение остаются обычными.",
		  "المشي والتنشين والسواقة بيفضلوا طبيعيين.", "行走、瞄准和驾驶保持正常。", "歩行・エイム・運転は通常どおりです。",
		  "Gehen, Zielen und Fahren normal bleiben.", "andar, mirar e dirigir continuam normais.", "berjalan, membidik, dan mengemudi tetap normal." },
		{ "FOV / distance / pivot come from the game's",
		  "Görüş alanı / mesafe / pivot oyunun", "FOV / distancia / pivote vienen de los", "Угол обзора / дистанция / центр берутся из",
		  "الـ FOV / البعد / المحور جايين من", "视野 / 距离 / 轴心来自游戏的", "視野 / 距離 / ピボットはゲームの",
		  "Sichtfeld / Distanz / Drehpunkt kommen aus den", "FOV / distância / pivô vêm dos", "FOV / jarak / pivot berasal dari" },
		{ "camera metadata; everything else is native.",
		  "kamera metadata'sından gelir; gerisi native.", "metadatos de cámara; lo demás es nativo.", "метаданных камеры; остальное — native.",
		  "بيانات الكاميرا (metadata)؛ والباقي natives.", "摄像机元数据；其余为原生。", "カメラメタデータ由来。他はネイティブです。",
		  "Kamerametadaten; alles andere sind Natives.", "metadados da câmera; o resto é nativo.", "metadata kamera; sisanya native." },
		{ "Settings and log are written next to the .asi.",
		  "Ayar ve kayıt dosyaları .asi yanına yazılır.", "Los ajustes y el registro se guardan junto al .asi.", "Настройки и журнал пишутся рядом с .asi.",
		  "الإعدادات وملف اللوج بيتكتبوا جنب ملف .asi.", "设置和日志写入 .asi 旁边。", "設定とログは .asi の隣に書き込まれます。",
		  "Einstellungen und Log liegen neben der .asi.", "Configurações e log ficam ao lado do .asi.", "Pengaturan dan log ditulis di sebelah .asi." },

		// ---- menu chrome -------------------------------------------------
		{ "Press any key to bind  -  ESC to cancel",
		  "Bir tuşa basın  -  ESC iptal", "Pulsa una tecla  -  ESC cancela", "Нажмите клавишу  -  ESC отмена",
		  "دوس أي زرار  -  ESC للإلغاء", "按下按键  -  ESC 取消", "キーを押してください  -  ESCで取消",
		  "Taste drücken  -  ESC zum Abbrechen", "Pressione uma tecla  -  ESC cancela", "Tekan tombol  -  ESC untuk batal" },
		{ "%s/%s move    %s/%s change",
		  "%s/%s gez    %s/%s değiştir", "%s/%s mover    %s/%s cambiar", "%s/%s навигация    %s/%s изменить",
		  "%s/%s تحرّك    %s/%s غيّر", "%s/%s 移动    %s/%s 调整", "%s/%s 移動    %s/%s 変更",
		  "%s/%s bewegen    %s/%s ändern", "%s/%s mover    %s/%s alterar", "%s/%s gerak    %s/%s ubah" },
		{ "%s select    %s back",
		  "%s seç    %s geri", "%s seleccionar    %s atrás", "%s выбрать    %s назад",
		  "%s اختيار    %s رجوع", "%s 选择    %s 返回", "%s 決定    %s 戻る",
		  "%s auswählen    %s zurück", "%s selecionar    %s voltar", "%s pilih    %s kembali" },
		{ "ON  -> the camera on screen is ours.",
		  "AÇIK  -> ekrandaki kamera bize ait.", "ACTIVADO  -> la cámara en pantalla es nuestra.", "ВКЛ  -> камера на экране под нашим управлением.",
		  "شغال  -> الكاميرا اللي على الشاشة بتاعتنا.", "开启  -> 屏幕上的摄像机由我们控制。", "オン  -> 画面のカメラは当MODが制御中。",
		  "EIN  -> die aktive Kamera wird gesteuert.", "LIGADO  -> a câmera na tela é controlada pelo mod.", "AKTIF  -> kamera di layar dikendalikan mod." },
		{ "ON",
		  "AÇIK", "ACTIVADO", "ВКЛ",
		  "شغال", "开启", "オン",
		  "EIN", "LIGADO", "AKTIF" },
		{ "OFF",
		  "KAPALI", "DESACTIVADO", "ВЫКЛ",
		  "مقفول", "关闭", "オフ",
		  "AUS", "DESLIGADO", "NONAKTIF" },
		{ "[ PRESS KEY ]",
		  "[ TUŞA BAS ]", "[ PULSA TECLA ]", "[ НАЖМИТЕ КЛАВИШУ ]",
		  "[ دوس زرار ]", "[ 按下按键 ]", "[ キーを押す ]",
		  "[ TASTE DRÜCKEN ]", "[ PRESSIONE TECLA ]", "[ TEKAN TOMBOL ]" },

		// ---- preset share code & extra cameras --------------------------
		{ "Share Preset Code",
		  "Önayar Kodunu Paylaş", "Compartir código de ajuste", "Поделиться кодом пресета",
		  "مشاركة كود الإعدادات", "分享预设代码", "プリセットコードを共有",
		  "Preset-Code teilen", "Compartilhar código de ajuste", "Bagikan Kode Preset" },
		{ "Export Code (Copy)",
		  "Kodu Dışa Aktar (Kopyala)", "Exportar código (Copiar)", "Экспорт кода (Копировать)",
		  "تصدير الكود (نسخ)", "导出代码（复制）", "コードを出力 (コピー)",
		  "Code exportieren (Kopieren)", "Exportar código (Copiar)", "Ekspor Kode (Salin)" },
		{ "Import Code (Paste)",
		  "Kodu İçe Aktar (Yapıştır)", "Importar código (Pegar)", "Импорт кода (Вставить)",
		  "استيراد الكود (لصق)", "导入代码（粘贴）", "コードを読み込む (貼り付け)",
		  "Code importieren (Einfügen)", "Importar código (Colar)", "Impor Kode (Tempel)" },
		{ "Files sit in scripts\\Cam Control\\:",
		  "Dosyalar scripts\\Cam Control\\ içinde:", "Los archivos están en scripts\\Cam Control\\:", "Файлы находятся в scripts\\Cam Control\\:",
		  "الملفات موجودة في scripts\\Cam Control\\:", "文件位于 scripts\\Cam Control\\：", "ファイルは scripts\\Cam Control\\ にあります:",
		  "Dateien liegen in scripts\\Cam Control\\:", "Arquivos ficam em scripts\\Cam Control\\:", "File berada di scripts\\Cam Control\\:" },
		{ "Other Scopes / Aim",
		  "Diğer Dürbünler / Nişan", "Otras miras / Apuntado", "Другие прицелы",
		  "السكوبات والتصويب التانية", "其他瞄准镜 / 瞄准", "その他のスコープ / 照準",
		  "Andere Visiere / Zielen", "Outras miras / Mira", "Scope / Bidik Lainnya" },
		{ "Extra Cameras",
		  "Ek Kameralar", "Cámaras extra", "Доп. камеры",
		  "كاميرات إضافية", "额外摄像机", "追加カメラ",
		  "Zusatzkameras", "Câmeras extras", "Kamera Tambahan" },
		{ "Reset Extra Cameras",
		  "Ek Kameraları Sıfırla", "Restablecer cámaras extra", "Сбросить доп. камеры",
		  "إعادة ضبط الكاميرات الإضافية", "重置额外摄像机", "追加カメラをリセット",
		  "Zusatzkameras zurücksetzen", "Redefinir câmeras extras", "Reset Kamera Tambahan" },
		{ "Parachute",
		  "Paraşüt", "Paracaídas", "Парашют",
		  "البراشوت", "降落伞", "パラシュート",
		  "Fallschirm", "Paraquedas", "Parasut" },
		{ "Parachute Field Of View",
		  "Paraşüt Görüş Açısı (FOV)", "Campo de visión en paracaídas", "FOV с парашютом",
		  "زاوية رؤية البراشوت (FOV)", "降落伞视野 (FOV)", "パラシュート視野角 (FOV)",
		  "Fallschirm-Sichtfeld (FOV)", "Campo de visão de paraquedas", "FOV Parasut" },
		{ "Parachute Follow Distance",
		  "Paraşüt Takip Mesafesi", "Distancia en paracaídas", "Дистанция камеры (парашют)",
		  "مسافة متابعة البراشوت", "降落伞跟随距离", "パラシュート追従距離",
		  "Fallschirm-Folgeabstand", "Distância de paraquedas", "Jarak Kamera Parasut" },
		{ "Vehicle Weapon Aiming",
		  "Araçtan Nişan Alma", "Apuntado desde vehículo", "Стрельба из транспорта",
		  "التصويب من جوا المركبة", "载具内武器瞄准", "乗り物からの射撃照準",
		  "Zielen aus Fahrzeugen", "Mira em veículos", "Membidik dari Kendaraan" },
		{ "Drive-By FOV (Cars/Bikes/Boats/Helis)",
		  "Araç İçi Nişan FOV (Araba/Motor/Tekne/Heli)", "FOV en vehículo (Coche/Moto/Barco/Heli)", "FOV из транспорта (Авто/Мото/Лодка/Верт.)",
		  "FOV التصويب من المركبات", "载具射击视野 (车/摩托/船/直升机)", "ドライブバイ視野角 (車/バイク/船/ヘリ)",
		  "Drive-By-FOV (Auto/Bike/Boot/Heli)", "FOV em veículos (Carro/Moto/Barco/Heli)", "FOV Drive-By (Mobil/Motor/Perahu/Heli)" },
	};

	const int kStringCount = (int)(sizeof(kStrings) / sizeof(kStrings[0]));

	Lang::Id g_current = Lang::English;
	std::wstring g_dir;

	// Overrides from lang_<code>.txt, which let a translation be corrected
	// without rebuilding the mod.
	std::map<std::string, std::string> g_override;
	std::set<std::string> g_reportedMissing;

	const char *Column(const Entry &e, Lang::Id id)
	{
		switch (id)
		{
		case Lang::Turkish:    return e.tr;
		case Lang::Spanish:    return e.es;
		case Lang::Russian:    return e.ru;
		case Lang::Arabic:     return e.ar;
		case Lang::Chinese:    return e.zh;
		case Lang::Japanese:   return e.ja;
		case Lang::German:     return e.de;
		case Lang::Portuguese: return e.pt;
		case Lang::Indonesian: return e.id;
		default:               return nullptr;
		}
	}

	std::string Trim(const std::string &s)
	{
		size_t a = 0;
		size_t b = s.size();
		while (a < b && (unsigned char)s[a] <= ' ')
			a++;
		while (b > a && (unsigned char)s[b - 1] <= ' ')
			b--;
		return s.substr(a, b - a);
	}

	// Reads lang_<code>.txt: "English text=translated text", one per line.
	void LoadOverride()
	{
		g_override.clear();

		if (g_dir.empty())
			return;

		const std::wstring code = Lang::Widen(Lang::Code(g_current));
		const std::wstring path = g_dir + L"lang_" + code + L".txt";

		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return;

		std::string data;
		char buf[4096];
		DWORD read = 0;
		while (ReadFile(h, buf, sizeof(buf), &read, nullptr) && read > 0)
			data.append(buf, read);
		CloseHandle(h);

		if (data.size() >= 3 && (unsigned char)data[0] == 0xEF &&
			(unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF)
			data.erase(0, 3);

		int count = 0;
		size_t pos = 0;
		while (pos <= data.size())
		{
			size_t eol = data.find('\n', pos);
			std::string line = (eol == std::string::npos) ? data.substr(pos) : data.substr(pos, eol - pos);
			pos = (eol == std::string::npos) ? data.size() + 1 : eol + 1;

			line = Trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#')
				continue;

			size_t eq = line.find('=');
			if (eq == std::string::npos)
				continue;

			const std::string key = Trim(line.substr(0, eq));
			const std::string value = Trim(line.substr(eq + 1));
			if (!key.empty() && !value.empty())
			{
				g_override[key] = value;
				count++;
			}
		}

		if (count > 0)
			Log::Line("lang: %d override(s) loaded from lang_%s.txt", count, Lang::Code(g_current));
	}
}

namespace Lang
{
	const char *Code(Id id)
	{
		switch (id)
		{
		case Turkish:    return "tr";
		case Spanish:    return "es";
		case Russian:    return "ru";
		case Arabic:     return "ar";
		case Chinese:    return "zh";
		case Japanese:   return "ja";
		case German:     return "de";
		case Portuguese: return "pt";
		case Indonesian: return "id";
		default:         return "en";
		}
	}

	// Latin names on purpose: the language list has to be readable in the mode
	// that is active NOW, and the native renderer cannot draw Arabic or CJK.
	// The menu runs these through Tr() so they localise too.
	const char *DisplayName(Id id)
	{
		switch (id)
		{
		case Turkish:    return "Turkish";
		case Spanish:    return "Spanish";
		case Russian:    return "Russian";
		case Arabic:     return "Egyptian Arabic";
		case Chinese:    return "Chinese";
		case Japanese:   return "Japanese";
		case German:     return "German";
		case Portuguese: return "Portuguese";
		case Indonesian: return "Indonesian";
		default:         return "English";
		}
	}

	bool IsRtl(Id id)
	{
		return id == Arabic;
	}

	bool NeedsCustomRender(Id id)
	{
		// Arabic and CJK cannot be drawn by the game at all.
		//
		// Spanish is grouped in for a different reason: it uses the same native
		// path as Turkish and Russian, yet in game its Latin-1 accents (a-acute,
		// n-tilde, the feminine ordinal) came out blank while Turkish and
		// Cyrillic drew fine. German and Portuguese carry the same kind of
		// accents (u-umlaut, sharp s, c-cedilla, a-tilde), so they take the
		// overlay too rather than depend on which glyphs the game's font happens
		// to carry. Indonesian is plain a-z, so it needs none of this.
		return id == Arabic || id == Chinese || id == Japanese ||
		       id == Spanish || id == German || id == Portuguese;
	}

	bool NeedsCustomRender()
	{
		return NeedsCustomRender(g_current);
	}

	const wchar_t *DxFontFamily(Id id)
	{
		switch (id)
		{
		case Chinese:  return L"Microsoft YaHei";
		case Japanese: return L"Yu Gothic";
		// Arial ships with Windows and carries full Arabic. DirectWrite does the
		// contextual shaping and the right-to-left ordering for us.
		case Arabic:   return L"Arial";
		default:       return L"Segoe UI";
		}
	}

	void Init(const std::wstring &baseDir)
	{
		g_current = English;
		g_dir = baseDir;
		g_reportedMissing.clear();
		LoadOverride();

		Log::Line("lang: %d strings, %d languages", kStringCount, (int)COUNT);
	}

	void Set(Id id)
	{
		if (id < 0 || id >= COUNT)
			id = English;

		g_current = id;
		LoadOverride();
	}

	Id Current()
	{
		return g_current;
	}

	const char *Tr(const char *key)
	{
		if (!key || !*key)
			return "";

		if (g_current == English)
			return key;

		// A hand-written override always wins.
		std::map<std::string, std::string>::const_iterator ov = g_override.find(key);
		if (ov != g_override.end())
			return ov->second.c_str();

		for (int i = 0; i < kStringCount; i++)
		{
			if (strcmp(kStrings[i].key, key) != 0)
				continue;

			const char *t = Column(kStrings[i], g_current);
			if (t && *t)
				return t;

			return key; // untranslated: fall back rather than show nothing
		}

		// Reported once so a typo in a call site shows up in the log instead of
		// silently rendering English forever.
		if (g_reportedMissing.insert(key).second)
			Log::Line("lang: no entry for '%s' - showing English", key);

		return key;
	}

	std::wstring Widen(const char *utf8)
	{
		if (!utf8 || !*utf8)
			return std::wstring();

		const int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (len <= 0)
			return std::wstring();

		std::wstring out((size_t)len - 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], len);
		return out;
	}

	std::string CurrentCode()
	{
		return std::string(Code(g_current));
	}
}
