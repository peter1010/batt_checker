pkgbase=batt_tracker
pkgname=('batt_tracker')
pkgver=1.0
pkgrel=1
pkgdesc="batt_tracker"
arch=('any')
url="http:"
license=('GPL')
makedepends=('python')
depends=('python' 'tkinter')
source=()
install='batt_checker.install'

pkgver() {
    python ../setup.py -V
}

check() {
    pushd ..
    python setup.py check
    popd
}

package() {
    pushd ../c_src
    make
    popd
    pushd ..
    DONT_START=1 python setup.py install --root=$pkgdir
    popd
}

