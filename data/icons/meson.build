# The Mavernix logo, installed into the icon theme every desktop falls back
# on, so that anything can show it by its name, "mavernix". Small sizes get
# a simplified drawing.

hicolor = get_option('datadir') / 'icons' / 'hicolor'
install_data('mavernix.svg', install_dir: hicolor / 'scalable' / 'apps')
foreach size : ['16x16', '22x22', '24x24', '32x32']
    install_data('mavernix-small.svg', install_dir: hicolor / size / 'apps', rename: 'mavernix.svg')
endforeach
