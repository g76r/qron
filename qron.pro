TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = libp6core libqron libqron/doc qrond libp6core/autodoc libqron/autodoc autodoc doc
linux {
  SUBDIRS += linux
}
