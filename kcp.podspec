Pod::Spec.new do |s|
  s.name             = 'KCP'
  s.version          = '1.0'
  s.license      = { :type => "MIT" }
  s.summary  = 'KCP'
  s.homepage = 'https://github.com/rannger/kcp'
  s.description = 'AZCategory'
  s.author           = { 'KCP' => 'liang.rannger' }
  s.source           = { :git => 'https://github.com/rannger/kcp.git', :tag => s.version.to_s }

  s.ios.deployment_target = '8.0'

  s.source_files = 'Classes/*.{h,c}'
end
