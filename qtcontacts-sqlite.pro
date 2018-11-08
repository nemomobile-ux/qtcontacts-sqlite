TEMPLATE = subdirs
SUBDIRS = \
        src
OTHER_FILES += rpm/qtcontacts-sqlite-qt5.spec

tests.depends = src
