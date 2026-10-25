#include "machine.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStorageInfo>
#include <QSysInfo>

#include <cmath>

// --- Reading small files ---------------------------------------------------------

// A whole file as text, without the white space around it; empty if it
// cannot be read.
static QString file_text(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

// The lines of a file; none if it cannot be read.
static QStringList file_lines(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QStringList();
    }
    return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
}

// A file of lines "KEY<separator>VALUE" as a table. Of a key that comes
// more than once the first value is kept (/proc/cpuinfo repeats its keys
// for every processor core). Quotes around a value are taken off.
static QMap<QString, QString> key_value_file(const QString& path, QChar separator)
{
    QMap<QString, QString> table;
    for (const QString& line : file_lines(path))
    {
        const int at = line.indexOf(separator);
        if (at < 0)
        {
            continue;
        }
        const QString key = line.left(at).trimmed();
        QString value = line.mid(at + 1).trimmed();
        if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
        {
            value = value.mid(1, value.size() - 2);
        }
        if (!table.contains(key))
        {
            table.insert(key, value);
        }
    }
    return table;
}

// A number with at most two places after the point and no zeros at its
// end: 2.1, 1.96, 3.
static QString short_number(double number)
{
    QString text = QString::number(number, 'f', 2);
    while (text.endsWith(QLatin1Char('0')))
    {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.')))
    {
        text.chop(1);
    }
    return text;
}

static const double bytes_in_a_gigabyte = 1024.0 * 1024.0 * 1024.0;

// --- Processor ---------------------------------------------------------------------

// A processor's name as its maker writes it into the chip, made short:
// "13th Gen Intel(R) Core(TM) i5-13420H" becomes "Intel Core i5-13420H",
// "AMD Ryzen 7 5800X 8-Core Processor" becomes "AMD Ryzen 7 5800X".
static QString short_processor_name(QString name)
{
    // The speed, where the name carries it, is told apart from the name.
    name.remove(QRegularExpression(QStringLiteral("@.*$")));
    name.remove(QRegularExpression(QStringLiteral("\\((R|TM|tm|r)\\)")));
    name.remove(QRegularExpression(QStringLiteral("\\b\\d+(st|nd|rd|th) Gen\\b")));
    name.remove(QRegularExpression(QStringLiteral("\\b\\d+-Core\\b")));
    name.remove(QRegularExpression(QStringLiteral("\\b(CPU|Processor)\\b")));
    return name.simplified();
}

// The processor's speed in gigahertz, or 0 if it cannot be found. In
// this order: the speed the maker wrote into the name ("@ 1.80GHz"), the
// speed the chip is rated for without its short bursts, the highest speed
// it can reach.
static double processor_gigahertz(const QString& full_name)
{
    const QRegularExpressionMatch in_name =
        QRegularExpression(QStringLiteral("@\\s*([0-9.]+)\\s*GHz")).match(full_name);
    if (in_name.hasMatch())
    {
        return in_name.captured(1).toDouble();
    }

    // Both files hold kilohertz.
    const double kilohertz_in_a_gigahertz = 1000000.0;
    const QString speed_folder = QStringLiteral("/sys/devices/system/cpu/cpu0/cpufreq/");
    for (const char* file : {"base_frequency", "cpuinfo_max_freq"})
    {
        const double kilohertz = file_text(speed_folder + QLatin1String(file)).toDouble();
        if (kilohertz > 0)
        {
            return kilohertz / kilohertz_in_a_gigahertz;
        }
    }
    return 0;
}

// How many processors are plugged into the board (not cores: chips).
static int processor_count()
{
    QSet<QString> chips;
    for (const QString& line : file_lines(QStringLiteral("/proc/cpuinfo")))
    {
        if (line.startsWith(QLatin1String("physical id")))
        {
            chips.insert(line.section(QLatin1Char(':'), 1).trimmed());
        }
    }
    return chips.isEmpty() ? 1 : chips.size();
}

QString Machine::processor() const
{
    const QMap<QString, QString> facts = key_value_file(QStringLiteral("/proc/cpuinfo"), QLatin1Char(':'));
    // "model name" on PCs; ARM boards say "Model" or "Hardware" or nothing.
    QString full_name = facts.value(QStringLiteral("model name"));
    if (full_name.isEmpty())
    {
        full_name = facts.value(QStringLiteral("Model"), facts.value(QStringLiteral("Hardware")));
    }
    if (full_name.isEmpty())
    {
        return QString();
    }

    QString line = short_processor_name(full_name);
    const double gigahertz = processor_gigahertz(full_name);
    if (gigahertz > 0)
    {
        line = short_number(gigahertz) + QStringLiteral(" GHz ") + line;
    }
    const int count = processor_count();
    if (count > 1)
    {
        line = QString::number(count) + QStringLiteral(" × ") + line;
    }
    return line;
}

// --- Memory ------------------------------------------------------------------------

// What the board's own description says about the memory modules. udev
// reads it at boot (it needs root for that) and leaves notes any user may
// read: lines like "E:MEMORY_DEVICE_0_SIZE=8589934592". No such file in a
// container or on a board without that description.
static QMap<QString, QString> memory_module_notes()
{
    QMap<QString, QString> notes;
    for (const QString& line : file_lines(QStringLiteral("/run/udev/data/+dmi:id")))
    {
        if (line.startsWith(QLatin1String("E:MEMORY_DEVICE_")))
        {
            notes.insert(line.mid(2).section(QLatin1Char('='), 0, 0), line.section(QLatin1Char('='), 1));
        }
    }
    return notes;
}

// The memory the system can use, rounded up to whole gigabytes: the
// kernel keeps a little of what is plugged in for itself, so its figure
// is always somewhat short of the modules' size.
static double usable_memory_gigabytes()
{
    const QMap<QString, QString> facts = key_value_file(QStringLiteral("/proc/meminfo"), QLatin1Char(':'));
    // "16098980 kB"
    const double kilobytes = facts.value(QStringLiteral("MemTotal")).section(QLatin1Char(' '), 0, 0).toDouble();
    return std::ceil(kilobytes * 1024.0 / bytes_in_a_gigabyte);
}

QString Machine::memory() const
{
    const QMap<QString, QString> notes = memory_module_notes();

    // The modules' sizes together, and the kind and speed of the first
    // module that tells them. A board has few sockets; 64 is far beyond.
    const int most_modules = 64;
    double module_bytes = 0;
    QString kind;
    QString speed;
    for (int module = 0; module < most_modules; module++)
    {
        const QString prefix = QStringLiteral("MEMORY_DEVICE_%1_").arg(module);
        module_bytes += notes.value(prefix + QStringLiteral("SIZE")).toDouble();
        if (kind.isEmpty())
        {
            kind = notes.value(prefix + QStringLiteral("TYPE"));
            // The speed the module runs at, or else the one it is made for.
            speed = notes.value(
                prefix + QStringLiteral("CONFIGURED_SPEED_MTS"), notes.value(prefix + QStringLiteral("SPEED_MTS")));
        }
    }

    const double gigabytes = module_bytes > 0 ? module_bytes / bytes_in_a_gigabyte : usable_memory_gigabytes();
    if (gigabytes <= 0)
    {
        return QString();
    }

    QString line = short_number(gigabytes) + QStringLiteral(" GB");
    if (kind.isEmpty() || kind == QLatin1String("Unknown") || kind == QLatin1String("Other"))
    {
        return line + QStringLiteral(" RAM");
    }
    if (!speed.isEmpty())
    {
        line += QStringLiteral(" ") + speed + QStringLiteral(" MHz");
    }
    return line + QStringLiteral(" ") + kind;
}

// --- Graphics ----------------------------------------------------------------------

// A maker's name from the list of PCI devices, made short: of
// "Advanced Micro Devices, Inc. [AMD/ATI]" the "AMD" in its brackets, of
// "Intel Corporation" and "NVIDIA Corporation" the first word.
static QString short_maker_name(const QString& name)
{
    const QRegularExpressionMatch bracket = QRegularExpression(QStringLiteral("\\[([^\\]/]+)")).match(name);
    if (bracket.hasMatch())
    {
        return bracket.captured(1).trimmed();
    }
    return name.section(QLatin1Char(' '), 0, 0);
}

// A device's name from that list, made short: the list writes a chip's
// code name first and the name it is sold under in brackets,
// "GP107M [GeForce GTX 1050 Ti Mobile]"; the bracket is what people know.
static QString short_device_name(const QString& name)
{
    const QRegularExpressionMatch bracket = QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]")).match(name);
    return bracket.hasMatch() ? bracket.captured(1).trimmed() : name;
}

// The name of a PCI device from its maker's and its own number (four hex
// digits each, as /sys gives them without the "0x"), looked up in the
// list of the `hwdata` package. Empty if the list or the device is not
// there.
//
// The list's form: a maker is a line "8086  Intel Corporation" at the
// line's start, its devices follow with one tab before them,
// "\ta7a8  Raptor Lake-P [UHD Graphics]".
static QString pci_device_name(const QString& maker_number, const QString& device_number)
{
    QStringList lines;
    for (const char* path : {"/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids"})
    {
        lines = file_lines(QLatin1String(path));
        if (!lines.isEmpty())
        {
            break;
        }
    }

    QString maker;
    for (const QString& line : lines)
    {
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
        {
            continue;
        }
        if (!line.startsWith(QLatin1Char('\t')))
        {
            if (!maker.isEmpty())
            {
                // The next maker begins: the device was not among ours.
                return maker;
            }
            if (line.startsWith(maker_number + QLatin1Char(' ')))
            {
                maker = short_maker_name(line.mid(maker_number.size()).trimmed());
            }
        }
        else if (!maker.isEmpty() && line.startsWith(QLatin1Char('\t') + device_number + QLatin1Char(' ')))
        {
            return maker + QLatin1Char(' ') + short_device_name(line.mid(1 + device_number.size()).trimmed());
        }
    }
    return maker;
}

QString Machine::graphics() const
{
    // The kernel lists every graphics card as /sys/class/drm/cardN (and
    // each card's plugs as cardN-NAME, which are not cards).
    const QDir cards_folder(QStringLiteral("/sys/class/drm"));
    const QRegularExpression card_name(QStringLiteral("^card[0-9]+$"));
    QStringList names;
    for (const QString& entry : cards_folder.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        if (!card_name.match(entry).hasMatch())
        {
            continue;
        }
        const QString device = cards_folder.filePath(entry) + QStringLiteral("/device/");
        // "0x8086": the list has the numbers without the "0x".
        const QString maker_number = file_text(device + QStringLiteral("vendor")).mid(2);
        const QString device_number = file_text(device + QStringLiteral("device")).mid(2);
        if (maker_number.isEmpty())
        {
            continue;
        }
        const QString name = pci_device_name(maker_number, device_number);
        if (name.isEmpty() || names.contains(name))
        {
            continue;
        }
        // The card the machine started its screen on comes first.
        if (file_text(device + QStringLiteral("boot_vga")) == QLatin1String("1"))
        {
            names.prepend(name);
        }
        else
        {
            names.append(name);
        }
    }
    return names.join(QStringLiteral(", "));
}

// --- The computer's model -------------------------------------------------------------

// Whether a text from the board's description says something. Makers of
// boards sold without a computer around them leave these in.
static bool says_something(const QString& text)
{
    static const char* empty_words[] = {"default string", "to be filled", "system product name", "system manufacturer",
        "system version", "not specified", "not applicable", "o.e.m", "none"};
    if (text.isEmpty())
    {
        return false;
    }
    for (const char* words : empty_words)
    {
        if (text.contains(QLatin1String(words), Qt::CaseInsensitive))
        {
            return false;
        }
    }
    return true;
}

QString Machine::computer() const
{
    const QString folder = QStringLiteral("/sys/class/dmi/id/");
    const QString maker = file_text(folder + QStringLiteral("sys_vendor"));
    QString model = file_text(folder + QStringLiteral("product_name"));
    // Lenovo writes a number of its own as the name ("20XW00FPGE") and the
    // name people know as the version ("ThinkPad X1 Carbon Gen 9").
    const QString version = file_text(folder + QStringLiteral("product_version"));
    if (maker.compare(QLatin1String("LENOVO"), Qt::CaseInsensitive) == 0 && says_something(version))
    {
        model = version;
    }

    QStringList parts;
    if (says_something(maker))
    {
        parts.append(maker);
    }
    // Some models' names begin with their maker's already.
    if (says_something(model))
    {
        parts.append(model.startsWith(maker, Qt::CaseInsensitive) ? model.mid(maker.size()).trimmed() : model);
    }
    return parts.join(QLatin1Char(' '));
}

// --- The disk the system started from -------------------------------------------------

QString Machine::startupDisk() const
{
    const QStorageInfo root = QStorageInfo::root();
    // The name its owner gave the file system, if any.
    if (!root.name().isEmpty())
    {
        return root.name();
    }

    // Else the model of the disk it is on. The kernel lists a partition
    // (nvme0n1p2) inside its disk's folder (…/block/nvme0n1/nvme0n1p2),
    // and the disk's folder has the model.
    const QString device = QFileInfo(QString::fromLocal8Bit(root.device())).fileName();
    const QString partition_folder = QFileInfo(QStringLiteral("/sys/class/block/") + device).canonicalFilePath();
    if (!partition_folder.isEmpty())
    {
        for (const QString& disk_folder : {QFileInfo(partition_folder).path(), partition_folder})
        {
            const QString model = file_text(disk_folder + QStringLiteral("/device/model"));
            if (!model.isEmpty())
            {
                return model;
            }
        }
    }
    // An encrypted or otherwise layered disk has no model of its own.
    return device;
}

// --- The system ----------------------------------------------------------------------

// The file in which a Linux system says what it is.
static QMap<QString, QString> system_description()
{
    QMap<QString, QString> description = key_value_file(QStringLiteral("/etc/os-release"), QLatin1Char('='));
    if (description.isEmpty())
    {
        description = key_value_file(QStringLiteral("/usr/lib/os-release"), QLatin1Char('='));
    }
    return description;
}

QString Machine::osName() const
{
    return system_description().value(QStringLiteral("NAME"), QStringLiteral("Linux"));
}

QString Machine::osVersion() const
{
    // Systems that are brought up to date continually (Arch) have none.
    return system_description().value(QStringLiteral("VERSION_ID"));
}

QString Machine::kernel() const
{
    return QSysInfo::kernelVersion();
}

QString Machine::hostName() const
{
    return QSysInfo::machineHostName();
}

QString Machine::fact(const QString& name) const
{
    if (name == QLatin1String("processor"))
        return processor();
    if (name == QLatin1String("memory"))
        return memory();
    if (name == QLatin1String("graphics"))
        return graphics();
    if (name == QLatin1String("computer"))
        return computer();
    if (name == QLatin1String("startup_disk"))
        return startupDisk();
    if (name == QLatin1String("os_name"))
        return osName();
    if (name == QLatin1String("os_version"))
        return osVersion();
    if (name == QLatin1String("kernel"))
        return kernel();
    if (name == QLatin1String("host_name"))
        return hostName();
    return QString();
}
