Name:       okb-engine
Summary:    OKboard engine
Version:    0.3
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

%install
rm -rf %{buildroot}

mkdir -p %{buildroot}/%{qml_maliit_dir} %{buildroot}/%{share_dir}
cp -p curve/build/libcurveplugin.so %{buildroot}/%{qml_maliit_dir}
cp predict.py %{buildroot}/%{qml_maliit_dir}
cp engine.version %{buildroot}/%{qml_maliit_dir}
cp okboard.cf %{buildroot}/%{share_dir}

tar xvfj %{SOURCE1}
tar xvfj %{SOURCE2}

for lang in fr en ; do
    cat $lang.tre | gzip -c > %{buildroot}/%{share_dir}/$lang.tre.gz
    cat predict-$lang.db | gzip -c > %{buildroot}/%{share_dir}/predict-$lang.db.gz
done

%files
%doc README.md LICENSE
%defattr(-,root,root,-)
%{qml_maliit_dir}/libcurveplugin.so
%{qml_maliit_dir}/predict.py*
%{qml_maliit_dir}/engine.version
%{share_dir}/okboard.cf

%files -n okb-lang-fr
%{share_dir}/fr.tre.gz
%{share_dir}/predict-fr.db.gz

%files -n okb-lang-en
%{share_dir}/en.tre.gz
%{share_dir}/predict-en.db.gz
