Name:       okb-engine
Summary:    OKboard engine
Version:    0.4.9
Release:    1
Group:      System/GUI/Other
License:    LGPLv2.1
URL:        file:///dev/null
Source0:    %{name}-%{version}.tar.gz
Source1:    okb-lang-fr.tar.bz2
Source2:    okb-lang-en.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  python3-devel

%define qml_maliit_dir /usr/share/maliit/plugins/eu/cpbm/okboard
%define share_dir /usr/share/okboard

%description
OKboard engine (curve typing analysis and word prediction)
No GUI or keyboard included

%package -n okb-lang-en
Summary:    English files for OKboard
Group:      System/GUI/Other
Requires:   %{name} = %{version}-%{release}
BuildArch:  noarch

%description -n okb-lang-en
English files for OKboard (dictionnary & prediction data)

%package -n okb-lang-fr
Summary:    French files for OKboard
Group:      System/GUI/Other
Requires:   %{name} = %{version}-%{release}
BuildArch:  noarch

%description -n okb-lang-fr
French files for OKboard (dictionnary & prediction data)

%prep
%setup -n %{name}-%{version}

%build
qmake
make -j 3
echo "%{version}-%{release} build: "`date` > engine.version

cd ngrams
python3 setup-fslm.py build_ext --inplace
python3 setup-cdb.py build_ext --inplace
mv cfslm*.so cfslm.so
mv cdb*.so cdb.so

%install
rm -rf %{buildroot}

mkdir -p %{buildroot}/%{qml_maliit_dir} %{buildroot}/%{share_dir}
cp -p curve/build/libcurveplugin.so %{buildroot}/%{qml_maliit_dir}
cp predict.py %{buildroot}/%{qml_maliit_dir}
cp language_model.py %{buildroot}/%{qml_maliit_dir}
cp backend.py %{buildroot}/%{qml_maliit_dir}
cp engine.version %{buildroot}/%{qml_maliit_dir}
cp okboard.cf %{buildroot}/%{share_dir}
cp -p ngrams/cfslm.so %{buildroot}/%{qml_maliit_dir}
cp -p ngrams/cdb.so %{buildroot}/%{qml_maliit_dir}

tar xvfj %{SOURCE1}
tar xvfj %{SOURCE2}

for lang in fr en ; do
    cat $lang.tre | gzip -c > %{buildroot}/%{share_dir}/$lang.tre.gz
    cat predict-$lang.db | gzip -c > %{buildroot}/%{share_dir}/predict-$lang.db.gz
    cat predict-$lang.ng | gzip -c > %{buildroot}/%{share_dir}/predict-$lang.ng.gz
    cp -f predict-$lang.id > %{buildroot}/%{share_dir}/predict-$lang.id
done

%files
%doc README.md LICENSE
%defattr(-,root,root,-)
%{qml_maliit_dir}/libcurveplugin.so
%{qml_maliit_dir}/predict.py*
%{qml_maliit_dir}/language_model.py*
%{qml_maliit_dir}/backend.py*
%{qml_maliit_dir}/cfslm.so
%{qml_maliit_dir}/cdb.so
%{qml_maliit_dir}/engine.version
%{share_dir}/okboard.cf

%files -n okb-lang-fr
%{share_dir}/fr.tre.gz
%{share_dir}/predict-fr.db.gz
%{share_dir}/predict-fr.ng.gz
%{share_dir}/predict-fr.id

%files -n okb-lang-en
%{share_dir}/en.tre.gz
%{share_dir}/predict-en.db.gz
%{share_dir}/predict-en.ng.gz
%{share_dir}/predict-en.id
