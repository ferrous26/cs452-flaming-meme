task default: :debug

task :increment_version do
  version = File.read('VERSION').to_i + 1
  File.open('VERSION', 'w') do |fd|
    fd.puts version
  end
end

task bench: :increment_version do
  sh 'BENCHMARK=yes make remote'
end

task debug: :increment_version do
  sh 'make remote'
end

task release: :increment_version do
  sh 'RELEASE=3 make remote'
end

task release_bench: :increment_version do
  sh 'RELEASE=3 BENCHMARK=yes make remote'
end

namespace :md5 do
  def make_md5_for report
    files = ['Makefile', 'Rakefile', 'orex.ld'] +
      FileList['include/**/*.h'] +
      FileList['src/**/*.c'] +
      FileList['src/**/*.asm']

    require 'digest/md5'
    require 'csv'

    md5 = Digest::MD5.new

    CSV.open("report/#{report}/md5_info.csv", 'w') do |csv|
      csv << ['file', 'hash']
      files.sort.each do |file|
        csv << [file.gsub(/_/, '\_'), md5.hexdigest(File.read file)]
      end
    end

    puts "md5sum -> report/#{report}/md5_info.csv"
  end

  task(:a0) { make_md5_for 'a0'       }
  task(:k1) { make_md5_for 'kernel1'  }
  task(:k2) { make_md5_for 'kernel2'  }
  task(:k3) { make_md5_for 'kernel3'  }
  task(:p1) { make_md5_for 'project1' }
  task(:p2) { make_md5_for 'project2' }
end

namespace :report do
  def pdf_compile report
    cd "report/#{report}" do
      sh 'pdflatex report.tex'
      sh 'pdflatex report.tex'
      sh 'open report.pdf'
    end
  end
  desc 'Compile the report for A0'
  task(a0: 'md5:a0') { pdf_compile 'a0'       }
  desc 'Compile the report for Kernel 1'
  task(k1: 'md5:k1') { pdf_compile 'kernel1'  }
  desc 'Compile the report for Kernel 2'
  task(k2: 'md5:k2') { pdf_compile 'kernel2'  }
  desc 'Compile the report for Kernel 3'
  task(k3: 'md5:k3') { pdf_compile 'kernel3'  }
  desc 'Compile the report for Project 1'
  task(p1: 'md5:p1') { pdf_compile 'project1' }
  desc 'Compile the report for Project 2'
  task(p2: 'md5:p2') { pdf_compile 'project2' }
end
