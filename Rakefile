task :default => :increment_version do
  sh 'make remote'
end

task :increment_version do
  version = File.read('VERSION').to_i + 1
  File.open('VERSION', 'w') do |fd|
    fd.puts version
  end
end

task :md5 do
  files = ['Makefile', 'Rakefile', 'orex.ld'] +
    FileList['include/*.h'] +
    FileList['src/*.c']

  require 'digest/md5'
  require 'csv'

  md5 = Digest::MD5.new

  CSV.open('reports/a0/md5_info.csv', 'w') do |csv|
    csv << ['file', 'hash']
    files.sort.each do |file|
      csv << [file.gsub(/_/, '\_'), md5.hexdigest(File.read file)]
    end
  end

  puts 'md5sum -> reports/a0/md5_info.csv'
end

task :a0 => :md5 do
  cd 'reports/a0' do
    sh 'pdflatex report.tex'
    sh 'pdflatex report.tex'
    sh 'open report.pdf'
  end
end
