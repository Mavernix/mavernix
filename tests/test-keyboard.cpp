// The keyboard as the settings window sees it (src/apps/settings): reading
// and writing shortcuts, and reading the list of keyboard layouts.

#include <QtTest>

#include <linux/input-event-codes.h>

#include "keyboard-layouts.hpp"
#include "shortcut-keys.hpp"

namespace
{
// Two layouts in the shape of xkeyboard-config's evdev.xml, one with a variant.
const char EXAMPLE_RULES[] = R"(<?xml version="1.0" encoding="UTF-8"?>
<xkbConfigRegistry version="1.1">
  <layoutList>
    <layout>
      <configItem>
        <name>us</name>
        <description>English (US)</description>
      </configItem>
      <variantList>
        <variant>
          <configItem>
            <name>intl</name>
            <description>English (US, intl., with dead keys)</description>
          </configItem>
        </variant>
      </variantList>
    </layout>
    <layout>
      <configItem>
        <name>de</name>
        <description>German</description>
      </configItem>
      <variantList/>
    </layout>
  </layoutList>
</xkbConfigRegistry>
)";

QString round_trip(const QString& shortcut)
{
    return write_shortcut(parse_shortcut(shortcut));
}
} // namespace

class TestKeyboard : public QObject
{
    Q_OBJECT

  private slots:
    void shortcuts_are_read_and_written_in_one_order()
    {
        QCOMPARE(round_trip(QStringLiteral("<ctrl> <super> KEY_F")), QStringLiteral("<ctrl> <super> KEY_F"));
        QCOMPARE(round_trip(QStringLiteral("<shift> <super> KEY_TAB")), QStringLiteral("<super> <shift> KEY_TAB"));
        QCOMPARE(round_trip(QStringLiteral("KEY_VOLUMEUP")), QStringLiteral("KEY_VOLUMEUP"));
    }

    void a_shortcut_may_have_alternatives()
    {
        const QString two = QStringLiteral("KEY_F3 | <ctrl> KEY_UP");
        QCOMPARE(round_trip(two), QStringLiteral("KEY_F3"));
        QCOMPARE(parse_alternatives(two).size(), 2);
        QVERIFY(has_alternative(two, parse_shortcut(QStringLiteral("<ctrl> KEY_UP"))));
        QVERIFY(!has_alternative(two, parse_shortcut(QStringLiteral("<alt> KEY_UP"))));
    }

    void nonsense_is_no_shortcut()
    {
        QCOMPARE(round_trip(QStringLiteral("<hyper> KEY_A")), QString());
        QCOMPARE(round_trip(QStringLiteral("<ctrl> A")), QString());
        QCOMPARE(round_trip(QStringLiteral("KEY_A KEY_B")), QString());
        QCOMPARE(round_trip(QString()), QString());
    }

    void every_offered_key_has_a_label_and_a_key_code()
    {
        for (const QString& key : choosable_keys())
        {
            QVERIFY2(!plain_key_label(key).isEmpty(), qPrintable(key));
            const int code = key_code_from_name(key);
            QVERIFY2(code > 0, qPrintable(key));
            QCOMPARE(key_name_from_code(code), key);
        }
        QCOMPARE(key_name_from_code(KEY_KP1), QString());
    }

    void modifiers_are_known_on_both_sides()
    {
        QCOMPARE(modifier_from_code(KEY_LEFTCTRL), QStringLiteral("ctrl"));
        QCOMPARE(modifier_from_code(KEY_RIGHTALT), QStringLiteral("alt"));
        QCOMPARE(modifier_from_code(KEY_LEFTSHIFT), QStringLiteral("shift"));
        QCOMPARE(modifier_from_code(KEY_RIGHTMETA), QStringLiteral("super"));
        QCOMPARE(modifier_from_code(KEY_A), QString());
    }

    void keys_programs_use_themselves_are_recognised()
    {
        QVERIFY(is_typing_key(QStringLiteral("KEY_A")));
        QVERIFY(!is_typing_key(QStringLiteral("KEY_F11")));

        const auto common = [](const char* text) {
            return is_common_program_key(parse_shortcut(QString::fromLatin1(text)));
        };
        QVERIFY(common("<ctrl> KEY_C"));
        QVERIFY(common("<ctrl> <shift> KEY_Z"));
        QVERIFY(common("<alt> KEY_F4"));
        QVERIFY(!common("<ctrl> <alt> KEY_C"));
        QVERIFY(!common("<super> KEY_C"));
    }

    void keyboard_layouts_are_read_and_sorted_by_name()
    {
        QTemporaryFile rules;
        QVERIFY(rules.open());
        rules.write(EXAMPLE_RULES);
        rules.flush();

        // A file that is missing is said, but does not stop the others being read.
        QString error;
        const KeyboardLayouts layouts =
            KeyboardLayouts::read({rules.fileName(), QStringLiteral("/nonexistent")}, &error);
        QCOMPARE(error, QString());
        QCOMPARE(layouts.all().size(), 3);
        QCOMPARE(layouts.describe(layouts.all().first()), QStringLiteral("English (US)"));
        QCOMPARE(layouts.describe(layouts.all().last()), QStringLiteral("German"));
        QCOMPARE(layouts.describe({QStringLiteral("us"), QStringLiteral("intl")}),
            QStringLiteral("English (US, intl., with dead keys)"));
        QCOMPARE(layouts.describe({QStringLiteral("xx"), QStringLiteral("yy")}), QStringLiteral("xx (yy)"));

        QString missing;
        KeyboardLayouts::read({QStringLiteral("/nonexistent")}, &missing);
        QVERIFY(!missing.isEmpty());
    }

    void the_configs_layout_texts_are_read_and_written()
    {
        const QList<InputSource> two = parse_sources(QStringLiteral("us,de"), QStringLiteral("intl,"));
        QCOMPARE(two.size(), 2);
        QCOMPARE(two[0].variant, QStringLiteral("intl"));
        QCOMPARE(two[1].variant, QString());
        QCOMPARE(layouts_text(two), QStringLiteral("us,de"));
        QCOMPARE(variants_text(two), QStringLiteral("intl,"));

        QCOMPARE(variants_text(parse_sources(QStringLiteral("us,de"), QString())), QString());
        QCOMPARE(parse_sources(QStringLiteral("us,,de"), QString()).size(), 2);
        QCOMPARE(parse_sources(QString(), QString()).size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestKeyboard)
#include "test-keyboard.moc"
